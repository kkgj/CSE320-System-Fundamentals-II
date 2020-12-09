#include <stdio.h>

#include "audio.h"
#include "debug.h"

int readIn(FILE *in) {
	uint32_t total = 0;
    int count = 0;
    while (count < 4) {
    	int temp = fgetc(in);
    	total += temp;
    	if (count == 3) {
    		break;
    	}
    	total = total << 8;
    	count++;
    }
    return total;
}

int audio_read_header(FILE *in, AUDIO_HEADER *hp) { // end of file error feof()
    hp -> magic_number = readIn(in);
    if (hp -> magic_number != AUDIO_MAGIC) {
    	return EOF;
    }
    hp -> data_offset = readIn(in);
    if (hp -> data_offset < AUDIO_DATA_OFFSET) {
    	return EOF;
	}
	hp -> data_size = readIn(in);
	hp -> encoding = readIn(in);
	if (hp -> encoding != PCM16_ENCODING) {
		return EOF;
	}
	hp -> sample_rate = readIn(in);
	if (hp -> sample_rate != AUDIO_FRAME_RATE) {
		return EOF;
	}
	hp -> channels = readIn(in);
	if (hp -> channels != AUDIO_CHANNELS) {
		return EOF;
	}
	if (hp -> data_offset == AUDIO_DATA_OFFSET) { // if offset is 24 return
		return 0;
	}
	int annotation = hp -> data_offset - AUDIO_DATA_OFFSET;
    //("%d\n", annotation);
	while (annotation != 0) {
		fgetc(in);
        annotation--;
	}
    return 0;
}

int writeOut(FILE *out, uint32_t head) {
	uint32_t bitwise = 0xFF000000;
	int shift = 24;
    while (shift >= 0) {
    	int temp = (bitwise & head);
    	fputc(temp >> shift, out);
    	shift -= 8;
    	bitwise = bitwise >> 8;
    }
    return 0;
}

int audio_write_header(FILE *out, AUDIO_HEADER *hp) {
    writeOut(out, hp -> magic_number);
    writeOut(out, hp -> data_offset);
    writeOut(out, hp -> data_size);
    writeOut(out, hp -> encoding);
    writeOut(out, hp -> sample_rate);
    writeOut(out, hp -> channels);
    return EOF;
}

int audio_read_sample(FILE *in, int16_t *samplep) {
    if (in == NULL || samplep == NULL || feof(in)) {
        return EOF;
    }
	int16_t first_byte = (fgetc(in) << 8);
	if (feof(in)) {
		return EOF;
	}
    *samplep = first_byte + fgetc(in);
    //printf("%hx\n", *samplep);
    return 0;
}

int audio_write_sample(FILE *out, int16_t sample) {
    fputc((sample & 0xFF00) >> 8, out);
    fputc((sample & 0xFF), out);
    return 0;
}
