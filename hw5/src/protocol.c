#include <arpa/inet.h>

#include "protocol.h"
#include "csapp.h"
#include "debug.h"

int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
	if (rio_writen(fd, hdr, sizeof(JEUX_PACKET_HEADER)) < 0) {
		return -1;
	}
	uint16_t size = ntohs(hdr->size);
	if (size > 0) {
		if (rio_writen(fd, data, size) < 0) {
			return -1;
		}
	}
	debug("=> Send payload type=%u, size=%u, id=%u, role=%u", hdr->type, size, hdr->id, hdr->role);
	return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
	if (rio_readn(fd, hdr, sizeof(JEUX_PACKET_HEADER)) <= 0) {
		return -1;
	}
	uint16_t size = ntohs(hdr->size);
	// hdr->size = ntohs(hdr->size);
	// hdr->timestamp_sec = ntohl(hdr->timestamp_sec);
	// hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);
	if (size > 0) {
		*payloadp = Calloc(size+1, sizeof(char));
		if (rio_readn(fd, *payloadp, size) <= 0) {
			return -1;
		}
	} else {
		*payloadp = NULL;
	}
	debug("<= Receive payload type=%u, size=%u, id=%u, role=%u", hdr->type, size, hdr->id, hdr->role);
	return 0;
}