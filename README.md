<p align="center">
<img src="http://demuxfs.sourceforge.net/demuxfs.png"/>
</p>

DemuxFS is a live filesystem that aids on the analysis of transport streams in digital TV systems. At the root of the filesystem, directories are created for each table in the transport stream, such as PMT, PAT, NIT, SDTT, and EIT. Inside these directories, virtual files reflect the many properties of these tables.

Packetized Elementary Streams (PES), which hold audio, video and data, are represented in DemuxFS as FIFOs. Through FIFOs, users can playback specific audio and video streams and inspect the contents of closed-caption streams. DemuxFS also handles the protocol stack of DSM-CC, allowing for the seamless inspection of interactive applications.

DemuxFS supports the SBTVD, ISDB, DVB and ATSC standards.

## Getting started

DemuxFS comes with two backends:

1. **filesrc**: lets you inspect a transport stream captured in a file

2. **linuxdvb**: lets you inspect a live transport stream through the LinuxDVB stack

### FILESRC backend

This is how you invoke DemuxFS to analyze the contents of a file. The directory at ```/Mount/DemuxFS``` will be populated with the data parsed from that file:

```shell
demuxfs -o backend=filesrc -o filesrc=/path/to/file /Mount/DemuxFS
```

You can also let DemuxFS continuously parse a file with the **fileloop** option:
```shell
demuxfs -o backend=filesrc -o filesrc=/path/to/file -o fileloop=-1 /Mount/DemuxFS
```

### LINUXDVB backend

By default, the LinuxDVB backend will attempt to configure the *frontend0*, *demux0*, and *dvr0* devices under ```/dev/dvb/adapter0```. If the frontend has been already tuned to a frequency by a third party program, then you can simply run:
```shell
demuxfs -o backend=linuxdvb /Mount/DemuxFS
```

DemuxFS can tune the frontend to a specified frequency, given in Hz, with:
```shell
demuxfs -o backend=linuxdvb -o frequency=527142857 /Mount/DemuxFS
```

The full list of options supported by this backend is given by ```demuxfs --help```

## Inspecting the transport stream

Once the transport stream has been mounted, its contents can be inspected with regular system utilities such as ```ls```, ```cat```, and ```getfattr```. The mount point holds one directory for each MPEG-2 TS table parsed by DemuxFS:

<img src="http://lucasvr.github.io/demuxfs/example-rootfs.svg"/>

### Table versions and their basic contents

When broadcasters need to announce changes to a program, they do so by sending a new *version* of a given table. DemuxFS records all versions received so far under different subdirectories. A symbolic link indicates the current version being broadcasted:

<img src="http://lucasvr.github.io/demuxfs/example-pat.svg"/>

Table members are stored as regular files and directories. The data format of each file is given by the **system.format** extended attribute:

<img src="http://lucasvr.github.io/demuxfs/example-getfattr.svg"/>

### MPEG-2 TS programs

In the MPEG-2 TS (transport stream), the PAT table announces the identification of the programs being broadcasted. The details of these programs are found in the PMT table(s). The PAT also announces the id of the current *network* (usually holding details about the broadcaster). For convenience, DemuxFS represents those as symbolic links to the PMT and NIT tables:

<img src="http://lucasvr.github.io/demuxfs/example-pat_symlinks.svg"/>

```AudioStreams``` and ```VideoStreams``` may hold more than one subdirectory each, as in programs with multiple camera angles or audio tracks. In that case, symbolic links named ```Primary``` and ```Secondary``` will designate the primary and secondary stream ids that applications are expected to use by default. Directories with all capital letters (such as ```PARENTAL_RATING_0```) represent table descriptors featured in the transport stream.

### [Packetized] Elementary Streams

Packetized elementary streams are data packets that include a header and a payload (often an audio, video, or caption stream). Elementary streams are the actual payload. Both are presented in DemuxFS as FIFO files. That is, one can inspect them with e.g., ```hexdump``` or reproduce them with e.g., ```ffplay``` and ```mplayer```.

DemuxFS also generates a preview of the video frame being currently received (or parsed) on-the-fly. When ```snapshot.gif``` is opened by an application, DemuxFS internally feeds [FFmpeg](https://ffmpeg.org) with the elementary stream data and copies the rendered frame back to that application.

This is how the contents of a H.264 video stream program look like:

<img src="http://lucasvr.github.io/demuxfs/example-pmt.svg"/>

Note that you need to invoke DemuxFS with ```-o parse_pes=1``` to enable still picture previews and raw access to the elementary stream.

### Data and object carousel

DemuxFS also handles the protocol stack of DSM-CC, which implements data and object carousels. All related tables (AIT, DII, DSI, and DDB) are exported to the filesystem. Besides, the actual data blocks are decoded and exported to the filesystem as regular files and directories. By doing so, users can inspect the contents of interactive applications and firmware updates. The decoded data is stored in the mount point's ```DSM-CC``` directory.

<img src="http://lucasvr.github.io/demuxfs/example-dsmcc.svg"/>
