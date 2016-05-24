<p align="center">
<img src="http://demuxfs.sourceforge.net/demuxfs.png"/>
</p>

DemuxFS is a live filesystem that aids on the analysis of transport streams in digital TV systems.
At the root of the filesystem, directories are created for each table in the transport stream, such as PMT, PAT, NIT, SDTT, EIT, among others. Inside these directories, virtual files map each field described by the MPEG-2 transport stream specification to the given table, such as program information, scrambling information and general options. This concept also extends to tables described by other standards such as SBTVD, ISDB, DVB and ATSC.

Packetized Elementary Streams (PES), which represent audio, video and data, are represented in DemuxFS as FIFOs. Throughout FIFOs, applications can playback specific audio and video streams and inspect the contents of closed-caption streams.
Video stream contents can also be verified through still picture snapshots. DemuxFS collects video PES packets and feeds the FFMpeg library, which decodes those buffers in software and returns back a raster snapshot of the current video frame in a special file.

DemuxFS also handles the protocol stack of DSM-CC, in which case both AIT, DII, DSI and DDB tables are exported to the filesystem. The DDB payload is decoded and exported to the filesystem, too, allowing for the inspection of interactive applications.


