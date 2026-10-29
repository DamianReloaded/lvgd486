#include "MidiFile.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

namespace vgd
{
    namespace
    {
        struct Reader
        {
            const unsigned char* Data; unsigned int Size, Pos;
            Reader(const unsigned char* p, unsigned int n) : Data(p), Size(n), Pos(0) {}
            bool ReadByte(unsigned int& v) { if (Pos >= Size) return false; v=Data[Pos++]; return true; }
            bool ReadBE16(unsigned int& v) {
                if (Size-Pos < 2) return false;
                v=((unsigned int)Data[Pos]<<8)|Data[Pos+1]; Pos+=2; return true;
            }
            bool ReadBE32(unsigned int& v) {
                if (Size-Pos < 4) return false;
                v=((unsigned int)Data[Pos]<<24)|((unsigned int)Data[Pos+1]<<16)|
                  ((unsigned int)Data[Pos+2]<<8)|Data[Pos+3]; Pos+=4; return true;
            }
            bool ReadVLQ(unsigned int& v) {
                v=0;
                for(int i=0;i<4;++i) {
                    unsigned int b; if(!ReadByte(b)) return false;
                    if(v > 0x01FFFFFFU) return false;
                    v=(v<<7)|(b&0x7F);
                    if(!(b&0x80)) return true;
                }
                return false;
            }
            bool Skip(unsigned int n) { if(n>Size-Pos)return false; Pos+=n; return true; }
        };
        static bool FourCC(const unsigned char* p, const char* s)
        { return p[0]==(unsigned char)s[0] && p[1]==(unsigned char)s[1] && p[2]==(unsigned char)s[2] && p[3]==(unsigned char)s[3]; }
    }

    MidiFile::MidiFile() : ErrorText("no error"), TrackCount(0), TicksPerQuarter(0), Format(0) {}
    bool MidiFile::Fail(const char* message) { ErrorText=message; return false; }

    bool MidiFile::Load(const char* path, MidiSong& song)
    {
        ErrorText="no error"; TrackCount=TicksPerQuarter=Format=0;
        if(!path || !*path) return Fail("invalid path");
        FILE* f=fopen(path,"rb"); if(!f) return Fail("could not open MIDI file");
        if(fseek(f,0,SEEK_END)!=0) { fclose(f); return Fail("file seek failed"); }
        long fileSize=ftell(f);
        if(fileSize<14 || fileSize>16*1024*1024L || fseek(f,0,SEEK_SET)!=0) { fclose(f); return Fail("invalid or too-large MIDI file"); }
        unsigned char* bytes=(unsigned char*)malloc((size_t)fileSize);
        if(!bytes) { fclose(f); return Fail("out of memory"); }
        size_t got=fread(bytes,1,(size_t)fileSize,f); fclose(f);
        if(got!=(size_t)fileSize) { free(bytes); return Fail("could not read complete MIDI file"); }

        Reader r(bytes,(unsigned int)fileSize); unsigned int headerLength=0, division=0;
        if(r.Size-r.Pos<4 || !FourCC(bytes,"MThd")) { free(bytes); return Fail("missing MThd header"); }
        r.Pos=4;
        if(!r.ReadBE32(headerLength) || headerLength<6 || headerLength>r.Size-r.Pos) { free(bytes); return Fail("invalid MIDI header length"); }
        unsigned int format=0, tracks=0;
        if(!r.ReadBE16(format)||!r.ReadBE16(tracks)||!r.ReadBE16(division)) { free(bytes); return Fail("truncated MIDI header"); }
        if(format>1) { free(bytes); return Fail("only MIDI format 0 and 1 are supported"); }
        if(!tracks || (format==0 && tracks!=1)) { free(bytes); return Fail("invalid track count for MIDI format"); }
        if(division&0x8000) { free(bytes); return Fail("SMPTE time division is not supported; use ticks-per-quarter MIDI"); }
        if(!division) { free(bytes); return Fail("invalid zero MIDI time division"); }
        if(!r.Skip(headerLength-6)) { free(bytes); return Fail("invalid extended MIDI header"); }
        song.Clear();
        if(!song.BeginTimeline(division)) { free(bytes); return Fail("could not initialize song timeline"); }

        unsigned int longestTick=0;
        for(unsigned int trackNo=0; trackNo<tracks; ++trackNo)
        {
            // Skip non-track chunks; honor each chunk's declared size.
            bool found=false; unsigned int chunkLength=0;
            while(r.Size-r.Pos>=8) {
                const unsigned char* id=bytes+r.Pos; r.Pos+=4;
                if(!r.ReadBE32(chunkLength) || chunkLength>r.Size-r.Pos) { free(bytes); return Fail("invalid chunk length"); }
                if(FourCC(id,"MTrk")) { found=true; break; }
                if(!r.Skip(chunkLength)) { free(bytes); return Fail("invalid chunk"); }
            }
            if(!found) { free(bytes); return Fail("missing MIDI track chunk"); }
            Reader tr(bytes+r.Pos,chunkLength); r.Pos+=chunkLength;
            unsigned int absoluteTick=0; unsigned int runningStatus=0;
            while(tr.Pos<tr.Size)
            {
                unsigned int delta=0, first=0;
                if(!tr.ReadVLQ(delta) || absoluteTick>UINT_MAX-delta) { free(bytes); return Fail("invalid event delta time"); }
                absoluteTick+=delta; if(absoluteTick>longestTick)longestTick=absoluteTick;
                if(!tr.ReadByte(first)) { free(bytes); return Fail("truncated MIDI event"); }
                unsigned int status=first, d1=0, d2=0;
                if(first<0x80) {
                    if(runningStatus<0x80 || runningStatus>=0xF0) { free(bytes); return Fail("invalid running status"); }
                    status=runningStatus; d1=first;
                } else if(status<0xF0) runningStatus=status;
                else runningStatus=0;

                if(status==0xFF) {
                    unsigned int type=0,len=0;
                    if(!tr.ReadByte(type)||!tr.ReadVLQ(len)||len>tr.Size-tr.Pos) { free(bytes); return Fail("invalid MIDI meta-event"); }
                    if(type==0x51 && len==3) {
                        unsigned int us=((unsigned int)tr.Data[tr.Pos]<<16)|((unsigned int)tr.Data[tr.Pos+1]<<8)|tr.Data[tr.Pos+2];
                        if(us && !song.AddTempoChange(absoluteTick,us)) { free(bytes); return Fail("could not store tempo event"); }
                    }
                    tr.Pos+=len;
                    if(type==0x2F) break;
                    continue;
                }
                if(status==0xF0 || status==0xF7) {
                    unsigned int len=0; if(!tr.ReadVLQ(len)||!tr.Skip(len)) { free(bytes); return Fail("invalid SysEx event"); }
                    continue;
                }
                if(status>=0xF0) { free(bytes); return Fail("unsupported system event in MIDI track"); }
                const unsigned int kind=status&0xF0;
                const unsigned int bytesNeeded=(kind==0xC0 || kind==0xD0)?1:2;
                if(first>=0x80) { if(!tr.ReadByte(d1)) { free(bytes); return Fail("truncated channel event"); } }
                if(bytesNeeded==2) { if(!tr.ReadByte(d2)) { free(bytes); return Fail("truncated channel event"); } }
                else d2=0;
                if(d1>127 || d2>127) { free(bytes); return Fail("invalid MIDI data byte"); }
                // Channel voice messages: note on/off, poly pressure, controllers,
                // program change, channel pressure, and pitch bend.
                if(kind>=0x80 && kind<=0xE0) {
                    if(!song.AddTimelineEvent(absoluteTick,(unsigned char)status,(unsigned char)d1,(unsigned char)d2)) {
                        free(bytes); return Fail("could not store MIDI channel event");
                    }
                }
            }
        }
        free(bytes);
        if(!song.FinishTimeline(longestTick)) return Fail("MIDI file contained no playable channel events");
        Format=format; TrackCount=tracks; TicksPerQuarter=division;
        return true;
    }
}
