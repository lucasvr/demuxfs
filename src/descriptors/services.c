#include "demuxfs.h"

const char *service_type_to_string(uint8_t service_type)
{
	switch (service_type) {
		case 0x00: return "Reserved for future use";
		case 0x01: return "Digital television service";
		case 0x02: return "Digital audio service";
		case 0x03: return "Teletext service";
		case 0x04: return "NVOD reference service";
		case 0x05: return "Time-shifted NVOD service";
		case 0x06: return "Mosaic service";
		case 0x07 ... 0x09: return "Reserved for future use";
		case 0x0a: return "Advanced codification for digital radio service";
		case 0x0b: return "Advanced codification for mosaic service";
		case 0x0c: return "Data transmission service";
		case 0x0d: return "Reserved for common-use interface (see EN 50221)";
		case 0x0e: return "RCS map (see EN 301 790)";
		case 0x0f: return "RCS FLS (see EN 301 790)";
		case 0x10: return "DVB MHP service";
		case 0x11: return "MPEG-2 HD digital TV service";
		case 0x12 ... 0x15: return "Reserved for future use";
		case 0x16: return "Advanced service codification for SD digital TV";
		case 0x17: return "Advanced service codification for SD time-shifted NVOD";
		case 0x18: return "Advanced service codification for SD NVOD reference";
		case 0x19: return "Advanced service codification for HD digital TV";
		case 0x1a: return "Advanced service codification for HD time-shifted NVOD";
		case 0x1b: return "Advanced service codification for HD NVOD reference";
		case 0x1c ... 0x7f: return "Reserved for future use";
		case 0x80 ... 0xa0: return "Defined by the service provider";
		case 0xa1: return "Special video service";
		case 0xa2: return "Special audio service";
		case 0xa3: return "Special data service";
		case 0xa4: return "Engineering service";
		case 0xa5: return "Promotional video service";
		case 0xa6: return "Promotional audio service";
		case 0xa7: return "Promotional data service";
		case 0xa8: return "Anticipated data storage service";
		case 0xa9: return "Data storage only service";
		case 0xaa: return "Bookmark service list";
		case 0xab: return "Simultaneous server-type service";
		case 0xac: return "File-independent service";
		case 0xad ... 0xbf: return "Undefined";
		case 0xc0: return "Data service";
		default: return "Undefined";
	}
}

