#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "const.h"
#include "audio.h"
#include "dtmf.h"
#include "dtmf_static.h"
#include "goertzel.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 */

/**
 * DTMF generation main function.
 * DTMF events are read (in textual tab-separated format) from the specified
 * input stream and audio data of a specified duration is written to the specified
 * output stream.  The DTMF events must be non-overlapping, in increasing order of
 * start index, and must lie completely within the specified duration.
 * The sample produced at a particular index will either be zero, if the index
 * does not lie between the start and end index of one of the DTMF events, or else
 * it will be a synthesized sample of the DTMF tone corresponding to the event in
 * which the index lies.
 *
 *  @param events_in  Stream from which to read DTMF events.
 *  @param audio_out  Stream to which to write audio header and sample data.
 *  @param length  Number of audio samples to be written.
 *  @return 0 if the header and specified number of samples are written successfully,
 *  EOF otherwise.
 */
int dtmf_generate(FILE *events_in, FILE *audio_out, uint32_t length) {
    if (events_in == NULL || audio_out == NULL) {
    	return EOF;
    }
    AUDIO_HEADER hp;
    hp.magic_number = 0x2e736e64;
    hp.data_offset = 0x18;
    hp.data_size = AUDIO_BYTES_PER_SAMPLE * length;
    hp.encoding = 0x3;
    hp.sample_rate = 8000;
    hp.channels = 0x1;
    audio_write_header(audio_out, &hp);
   	int e = 0;
   	int noise_exist = 0;
   	FILE* noise = NULL;
   	int16_t noise_sample = 0;
   	if (noise_file != NULL) {
   		noise = fopen(noise_file, "r");
   		noise_exist = 1;
   		int temp = audio_read_header(noise, &hp);
   		if (temp == EOF) {
   			return EOF;
   		}
   		//printf("%d\n", h.data_offset);
   	}
   	while(fgets(line_buf, LINE_BUF_SIZE, events_in)) {
   		int start = 0;
   		int end = 0;
   		int count = 0;
   		while (*(line_buf + count) != '\t') {
   			start += *(line_buf + count) - 48;
   			count++;
   			if (*(line_buf + count) == '\t') {
   				count++;
   				break;
   			}
   			start = start * 10;
   		}
	    	// printf("start is %d\n", start);
   		while (*(line_buf + count) != '\t') {
   			end += *(line_buf + count) - 48;
   			count++;
   			if (*(line_buf + count) == '\t') {
   				count++;
   				break;
   			}
   			end = end * 10;
   		}
	    	// printf("end is %d\n", end);
   		if (e > start || start >= end) {
   			fprintf(stderr, "Overlapping error\n");
   			return EOF;
   		}
   		if (start >= length) {
   			break;
   		}
   		int zeros = start - e;
   		while (zeros > 0) {
   			if (noise_exist) {
   				if (audio_read_sample(noise, &noise_sample) == EOF) {
   					audio_write_sample(audio_out, 0);
   				} else {
   					double w = pow(10, 0.1 * noise_level) / (pow(10, 0.1 * noise_level) + 1);
   					int16_t combine = (int16_t) (w * noise_sample);
   					audio_write_sample(audio_out, combine);
   				}
   			} else {
   				audio_write_sample(audio_out, 0);
   			}
   			zeros--;
   		}
   		e = end;
   		char symbol = *(line_buf + count);
	    	// printf("%c\n", symbol);
   		int r = -1;
   		int c = -1;
   		for (int i = 0; i < NUM_DTMF_ROW_FREQS; i++) {
   			for (int j = 0; j < NUM_DTMF_COL_FREQS; j++) {
   				if (*(*(dtmf_symbol_names + i) + j) == symbol) {
   					r = i;
   					c = j;
   					break;
   				}
   			}
   			if (r != -1 || c != -1) {
   				break;
   			}
   		}
	    	// printf("Fr is %d, Fc is %d\n", Fr, Fc);
   		if (r == -1 || c == -1) {
   			return EOF;
   		}
   		int Fr = *(dtmf_freqs + r);
   		int Fc = *(dtmf_freqs + NUM_DTMF_ROW_FREQS + c);
	    	// printf("s: %d, e: %d, Fr %d, Fc %d\n", start, end, Fr, Fc);
   		for (int i = start; i < end && i < length; i++) {
   			double a = cos(2.0 * M_PI * Fr * i / AUDIO_FRAME_RATE);
   			double b = cos(2.0 * M_PI * Fc * i / AUDIO_FRAME_RATE);
   			int16_t sample = (int16_t)((a + b) * 0.5 * INT16_MAX);
   			if (noise_exist) {
   				if (audio_read_sample(noise, &noise_sample) == EOF) { // This function returns zero if successful, or else it returns a non-zero value.
   					double w = pow(10, 0.1 * noise_level) / (pow(10, 0.1 * noise_level) + 1);
   					int16_t combine = (int16_t) ((a + b) * 0.5 * INT16_MAX * (1 - w));
   					audio_write_sample(audio_out, combine);
   				} else {
   					double w = pow(10, 0.1 * noise_level) / (pow(10, 0.1 * noise_level) + 1);
   					int16_t combine = (int16_t) (w * noise_sample + (a + b) * 0.5 * INT16_MAX * (1 - w));
   					audio_write_sample(audio_out, combine);
   				}
   			} else {
   				audio_write_sample(audio_out, sample);
   			}
   		}
   		if (end >= length) {
   			e = length;
   			break;
   		}
   	}
   	while (e < length) {
   		if (noise_exist) {
   			if (audio_read_sample(noise, &noise_sample) == EOF) { // This function returns zero if successful, or else it returns a non-zero value.
   				audio_write_sample(audio_out, 0);
  			} else {
   				double w = pow(10, 0.1 * noise_level) / (pow(10, 0.1 * noise_level) + 1);
   				int16_t combine = (int16_t) (w * noise_sample);
   				audio_write_sample(audio_out, combine);
   			}
   		} else {
   			audio_write_sample(audio_out, 0);
   		}
  		e++;
   	}
   	return 0;
}

int findStrong(int* row_index, int* col_index, double* strong_row, double* strong_col) {
	for (int i = 0; i < 4; i++) {
		if (*(goertzel_strengths + i) > *strong_row) {
			*strong_row = *(goertzel_strengths + i);
			*row_index = i;
		}
		if (*(goertzel_strengths + i + 4) > *strong_col) {
			*strong_col = *(goertzel_strengths + i + 4);
			*col_index = i;
		}
	}
	return 0;
}

int checkSixDB(int row_index, int col_index) {
	for (int i = 0; i < NUM_DTMF_ROW_FREQS; i++) {
    	if ((i != row_index) && (*(goertzel_strengths + row_index) < *(goertzel_strengths + i) * SIX_DB)) {
    		return 0;
   		}
   		if ((i != col_index) && (*(goertzel_strengths + 4 + col_index) < *(goertzel_strengths + i + 4) * SIX_DB)) {
    		return 0;
    	}
   	}
   	return 1;
}

int goertzel_generate(FILE* audio_in, int N) {
	for (int F = 0; F < NUM_DTMF_FREQS; F++) {
		double k = *(dtmf_freqs + F) * N * 1.0 / AUDIO_FRAME_RATE;
		goertzel_init(goertzel_state + F, N, k);
	}
	int16_t sample;
	for(int i = 0; i < N-1; i++) {
		int temp = audio_read_sample(audio_in, &sample);
		if (temp == EOF) {
			return 0;
		}
		for (int F = 0; F < NUM_DTMF_FREQS; F++) {
			goertzel_step(goertzel_state + F, (double)sample / INT16_MAX);
		}
	}
	int tem = audio_read_sample(audio_in, &sample);
	if (tem == EOF) {
		return 0;
	}
	for (int F = 0; F < NUM_DTMF_FREQS; F++) {
		double r = goertzel_strength(goertzel_state + F, (double)sample / INT16_MAX);
		*(goertzel_strengths + F) = r;
	}
	return 1;
}

/**
 * DTMF detection main function.
 * This function first reads and validates an audio header from the specified input stream.
 * The value in the data size field of the header is ignored, as is any annotation data that
 * might occur after the header.
 *
 * This function then reads audio sample data from the input stream, partititions the audio samples
 * into successive blocks of block_size samples, and for each block determines whether or not
 * a DTMF tone is present in that block.  When a DTMF tone is detected in a block, the starting index
 * of that block is recorded as the beginning of a "DTMF event".  As long as the same DTMF tone is
 * present in subsequent blocks, the duration of the current DTMF event is extended.  As soon as a
 * block is encountered in which the same DTMF tone is not present, either because no DTMF tone is
 * present in that block or a different tone is present, then the starting index of that block
 * is recorded as the ending index of the current DTMF event.  If the duration of the now-completed
 * DTMF event is greater than or equal to MIN_DTMF_DURATION, then a line of text representing
 * this DTMF event in tab-separated format is emitted to the output stream. If the duration of the
 * DTMF event is less that MIN_DTMF_DURATION, then the event is discarded and nothing is emitted
 * to the output stream.  When the end of audio input is reached, then the total number of samples
 * read is used as the ending index of any current DTMF event and this final event is emitted
 * if its length is at least MIN_DTMF_DURATION.
 *
 *   @param audio_in  Input stream from which to read audio header and sample data.
 *   @param events_out  Output stream to which DTMF events are to be written.
 *   @return 0  If reading of audio and writing of DTMF events is sucessful, EOF otherwise.
 */
int dtmf_detect(FILE *audio_in, FILE *events_out) {
    if (audio_in == NULL || events_out == NULL) {
    	return EOF;
    }
    AUDIO_HEADER hp;
    int check_header = audio_read_header(audio_in, &hp);
    if (check_header == EOF) {
    	return EOF;
    }
    int start = -1;
    int index = 0;
    uint8_t tone = 0;
    while (feof(audio_in) != EOF) {
    	int valid = 1;
    	int valid_block = goertzel_generate(audio_in, block_size);
    	if (!valid_block && start != -1) {
    		if ((double)(index - start)/8000 >= MIN_DTMF_DURATION) {
    			fprintf(events_out, "%i\t%i\t%c\n", start, index, tone);
    		}
    		break;
    	} else if (!valid_block) {
    		break;
    	}
    	double strong_row = 0;
    	double strong_col = 0;
    	int row_index = 0;
    	int col_index = 0;
    	findStrong(&row_index, &col_index, &strong_row, &strong_col);
    	if (strong_row + strong_col < MINUS_20DB) {
    		valid = 0;
    	}
    	double ratio = strong_row / strong_col;
    	if (ratio > FOUR_DB || ratio < 1/FOUR_DB) {
    		valid = 0;
    	}
    	valid = checkSixDB(row_index, col_index) ? valid : 0;
    	if (valid && start == -1) {
    		start = index;
    		tone = *(*(dtmf_symbol_names + row_index) + col_index);
    	} else if ((!valid || tone != *(*(dtmf_symbol_names + row_index) + col_index)) && start != -1) {
    		if ((double)(index - start)/8000 >= MIN_DTMF_DURATION) {
    			fprintf(events_out, "%i\t%i\t%c\n", start, index, tone);
    		}
    		if (valid && tone != *(*(dtmf_symbol_names + row_index) + col_index)) {
    			start = index;
    			tone = *(*(dtmf_symbol_names + row_index) + col_index);
    		} else {
    			start = -1;
	    		tone = 0;
    		}
    	}
    	index += block_size;
    }
    fflush(events_out);
    return 0;
}

/**
 * Return the length of a string
 */
int len(char *string) {
	int count = 0;
	while (*string != 0) {
		string++;
		count++;
	}
	return count;
}

/**
 * Compare two strings
 */
int equal(char *a, char *b) {
	int aLen = len(a);
	if (aLen != len(b)) {
		return 0;
	}
	for (int i = 0; i < aLen; i++) {
		if (*a != *b) {
			return 0;
		}
		a++;
		b++;
	}
	return 1;
}

/**
 * Parse String
 */
int parse(char *a) {
	int count = 0;
	while (*a != 0) {
		int num = *a - '0';
		if (num < 0 || num > 9) {
			return -1;
		}
		count += num;
		a++;
		if (*a == 0) {
			break;
		}
		count = count * 10;
	}
	return count;
}

void setH() {
	global_options = HELP_OPTION;
}

void setG() {
	global_options = GENERATE_OPTION;
}

void setD() {
	global_options = DETECT_OPTION;
}

void setErr() {
	global_options = 0x0;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the operation mode of the program (help, generate,
 * or detect) will be recorded in the global variable `global_options`,
 * where it will be accessible elsewhere in the program.
 * Global variables `audio_samples`, `noise file`, `noise_level`, and `block_size`
 * will also be set, either to values derived from specified `-t`, `-n`, `-l` and `-b`
 * options, or else to their default values.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected program operation mode, and global variables `audio_samples`,
 * `noise file`, `noise_level`, and `block_size` to contain values derived from
 * other option settings.
 */
int validargs(int argc, char **argv)
{
	// debug("%d", argc);
	setErr(); // global_options 0
	if (argc < 2) {
		return -1;
	}
	char *first = *(argv + 1);
	if (equal(first, "-h")) {
		setH();
		return 0;
	}
	if (argc > 8 || argc % 2 == 1) { // after -h failed and arc exceeds the limit
		return -1;
	}
	if (equal(first, "-g")) {
		noise_file = NULL;
		noise_level = 0;
		if (argc == 2) {
			setG();
			audio_samples = 8000; // Default MSEC 1000 time
			return 0;
		} else if (equal(*(argv + 2), "-t")) {
			char * MSEC_string = *(argv + 3);
			int MSEC = parse(MSEC_string);
			if (MSEC <= UINT32_MAX && MSEC >= 0) { // MSEC <= UINT32_MAX && 2,147,483,647 / 8
				audio_samples = MSEC * 8;
				if (audio_samples < 0) {
					return -1;
				}
				if (argc == 4) {
					setG();
					return 0;
				} else if (equal(*(argv + 4), "-n")) {
					noise_file = *(argv + 5);
					if (argc == 6) {
						setG();
						return 0;
					} else if (equal(*(argv + 6), "-l")) {
						char * level = *(argv + 7);
						int bool = 0; // is negative?
						if (*level == 45) {
							bool = 1;
							level++;
						}
						int count = 0;
						while (*level != 0) {
							int num = *level - '0';
							if (num < 0 || num > 9) {
								return -1;
							}
							count += num;
							level++;
							if (*level == 0) {
								break;
							}
							count = count * 10;
						}
						if (bool) {
							count = 0 - count;
						}
						if (count >= -30 && count <= 30) {
							noise_level = count;
							setG();
							return 0;
						} else {
							return -1;
						}
					} else {
						return -1;
					}
				} else if (equal(*(argv + 4), "-l")) { // l and n
					char * level = *(argv + 5);
					int bool = 0; // is negative?
					if (*level == 45) {
						bool = 1;
						level++;
					}
					int count = 0;
					while (*level != 0) {
						int num = *level - '0';
						if (num < 0 || num > 9) {
							return -1;
						}
						count += num;
						level++;
						if (*level == 0) {
							break;
						}
						count = count * 10;
					}
					if (bool) {
						count = 0 - count;
					}
					if (count >= -30 && count <= 30) {
						noise_level = count;
						if (argc == 6) {
							setG();
							return 0;
						} else if (equal(*(argv + 6), "-n")) {
							noise_file = *(argv + 7);
							setG();
							return 0;
						} else {
							return -1;
						}
					} else {
						return -1;
					}
				} else {
					return -1;
				}
			} else {
				return -1;
			}
		} else if (equal(*(argv + 2), "-n")) {
			noise_file = *(argv + 3);
			if (argc == 4) {
				setG();
				audio_samples = 8000; // Default MSEC 1000 time
				return 0;
			} else if (equal(*(argv + 4), "-t")){
				int MSEC = parse(*(argv + 5));
				if (MSEC <= UINT32_MAX && MSEC >= 0) { // MSEC <= UINT32_MAX && 2,147,483,647 / 8
					audio_samples = MSEC * 8;
					if (audio_samples < 0) {
					return -1;
					}
					if (argc == 6) {
						setG();
						return 0;
					} else if (equal(*(argv + 6), "-l")) {
						char * level = *(argv + 7);
						int bool = 0; // is negative?
						if (*level == 45) {
							bool = 1;
							level++;
						}
						int count = 0;
						while (*level != 0) {
							int num = *level - '0';
							if (num < 0 || num > 9) {
								return -1;
							}
							count += num;
							level++;
							if (*level == 0) {
								break;
							}
							count = count * 10;
						}
						if (bool) {
							count = 0 - count;
						}
						if (count >= -30 && count <= 30) {
							noise_level = count;
							setG();
							return 0;
						} else {
							return -1;
						}
					} else {
						return -1;
					}
				} else {
					return -1;
				}
			} else if (equal(*(argv + 4), "-l")){ // l and t
				char * level = *(argv + 5);
				int bool = 0; // is negative?
				if (*level == 45) {
					bool = 1;
					level++;
				}
				int count = 0;
				while (*level != 0) {
					int num = *level - '0';
					if (num < 0 || num > 9) {
						return -1;
					}
					count += num;
					level++;
					if (*level == 0) {
						break;
					}
					count = count * 10;
				}
				if (bool) {
					count = 0 - count;
				}
				if (count >= -30 && count <= 30) {
					noise_level = count;
					if (argc == 6) {
						setG();
						audio_samples = 8000; // Default MSEC 1000 time
						return 0;
					} else if (equal(*(argv + 6), "-t")) {
						int MSEC = parse(*(argv + 7));
						if (MSEC <= UINT32_MAX && MSEC >= 0) { // MSEC <= UINT32_MAX && 2,147,483,647 / 8
							audio_samples = MSEC * 8;
							if (audio_samples < 0) {
								return -1;
							}
							setG();
							return 0;
						} else {
							return -1;
						}
					} else {
						return -1;
					}
				} else {
					return -1;
				}
			} else {
				return -1;
			}
		} else if (equal(*(argv + 2), "-l")) {
			char * level = *(argv + 3);
			int bool = 0; // is negative?
			if (*level == 45) {
				bool = 1;
				level++;
			}
			int count = 0;
			while (*level != 0) {
				int num = *level - '0';
				if (num < 0 || num > 9) {
					return -1;
				}
				count += num;
				level++;
				if (*level == 0) {
					break;
				}
				count = count * 10;
			}
			if (bool) {
				count = 0 - count;
			}
			if (count >= -30 && count <= 30) {
				noise_level = count;
				if (argc == 4) {
					setG();
					audio_samples = 8000; // Default MSEC 1000 time
					return 0;
				} else if (equal(*(argv + 4), "-t")) {
					int MSEC = parse(*(argv + 5));
					if (MSEC <= UINT32_MAX && MSEC >= 0) { // MSEC <= UINT32_MAX && 2,147,483,647 / 8
						audio_samples = MSEC * 8;
						if (audio_samples < 0) {
							return -1;
						}
						if (argc == 6) {
							setG();
							return 0;
						} else if (equal(*(argv + 6), "-n")) {
							noise_file = *(argv + 7);
							setG();
							return 0;
						} else {
							return -1;
						}
					} else {
						return -1;
					}
				} else if (equal(*(argv + 4), "-n")) {
					noise_file = *(argv + 5);
					if (argc == 6) {
						setG();
						audio_samples = 8000; // Default MSEC 1000 time
						return 0;
					} else if (equal(*(argv + 6), "-t")) {
						int MSEC = parse(*(argv + 7));
						if (MSEC <= UINT32_MAX && MSEC >= 0) { // MSEC <= UINT32_MAX && 2,147,483,647 / 8
							audio_samples = MSEC * 8;
							if (audio_samples < 0) {
							return -1;
							}
							setG();
							return 0;
						} else {
							return -1;
						}
					} else {
						return -1;
					}
				} else {
					return -1;
				}
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	}
	if (equal(first, "-d")) {
		// printf("%s\n", "Got detect");
		if (argc > 4) {
			return -1;
		}
		if (argc == 2) {
			setD();
			block_size = DEFAULT_BLOCK_SIZE;
			return 0;
		} else if (equal(*(argv + 2), "-b")) {
			// char *blockSize = *(argv + 3);
			int count = parse(*(argv + 3));
			if (count >= 10 && count <= 1000) {
				block_size = count;
				setD();
				return 0;
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	}
	return -1;
}
