#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
#include "MidiFile.h" // Using midifile library
#include <ppl.h>
#include <mutex>
#include <Windows.h>

using namespace smf;
using namespace std;
using namespace concurrency;

struct MidiNote {
    uint32_t start;
    uint8_t vol;
};

struct FL_Event {
    uint32_t pos;
    uint16_t flags;
    uint16_t rack;
    uint32_t dur;
    uint8_t key;
    uint8_t unk1;
    uint8_t unk2;
    uint8_t chan;
    uint8_t unk3;
    uint8_t vol;
    uint8_t unk4;
    uint8_t unk5;
};

mutex MutexLock;

string OpenMidiFileDialog()
{
    OPENFILENAMEA ofn;
    char szFile[1<<10] = {};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "MIDI Files\0*.MID;*.MIDI\0All Files\0*.*\0";
    ofn.lpstrTitle = "midi2flp v1.03";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        return string(ofn.lpstrFile);
    }
    return "";
}

void Make_FL_Event(vector<uint8_t>& FLdt_Data, uint8_t Value, vector<uint8_t> Data) {
    FLdt_Data.push_back(Value);
    if (Value < 1 << 6) { // int8
        Data.resize(1, 0); // Fill the rest of the space with 0 if the data length is shorter than what we expect. 
        FLdt_Data.insert(FLdt_Data.end(), Data.begin(), Data.begin() + 1);
    }
    else if (Value < 1 << 7) { // int16
        Data.resize(2, 0); // Fill the rest of the space with 0 if the data length is shorter than what we expect. 
        FLdt_Data.insert(FLdt_Data.end(), Data.begin(), Data.begin() + 2);
    }
    else if (Value < (1 << 8) - (1 << 6)) { // int32
        Data.resize(3, 0); // Fill the rest of the space with 0 if the data length is shorter than what we expect. 
        FLdt_Data.insert(FLdt_Data.end(), Data.begin(), Data.begin() + 4);
    }
    else if (Value < (1 << 8) - (1 << 5)) { // text
        size_t DataSize = Data.size();
        while (DataSize >= 0x80) {
            FLdt_Data.push_back(static_cast<uint8_t>((DataSize & 0x7F) | 0x80));
            DataSize >>= 7;
        }
        FLdt_Data.push_back(static_cast<uint8_t>(DataSize));
        FLdt_Data.insert(FLdt_Data.end(), Data.begin(), Data.end());
    }
    else { // data
        size_t DataSize = Data.size();
        while (DataSize >= 0x80) {
            FLdt_Data.push_back(static_cast<uint8_t>((DataSize & 0x7F) | 0x80));
            DataSize >>= 7;
        }
        FLdt_Data.push_back(static_cast<uint8_t>(DataSize));
        FLdt_Data.insert(FLdt_Data.end(), Data.begin(), Data.end());
    }
}

vector<uint8_t> TO_BYTES_BIG_UINT8(uint64_t value, uint8_t length) {
    vector<uint8_t> bytes(length, 0);
    for (int i = length - 1; i >= 0; --i) {
        bytes[i] = static_cast<uint8_t>(value & 0xFF);
        value >>= 8;
    }
    return bytes;
}

vector<uint8_t> TO_BYTES_LITTLE_UINT8(uint64_t value, uint8_t length) {
    vector<uint8_t> bytes(length, 0);
    for (int i = 0; i < length; ++i) {
        bytes[i] = static_cast<uint8_t>(value & 0xFF);
        value >>= 8;
    }
    return bytes;
}

int main(int argc, char** argv) {
    string FileName = OpenMidiFileDialog();
    if (FileName.length() > 0) {
        MidiFile Input_MIDI;
        cout << "Loading MIDI file... \n";
        Input_MIDI.read(FileName);
        ofstream Output_FLP(FileName + ".flp", ios::binary);
        vector<uint8_t> FLhd_Data;
        FLhd_Data.push_back(0x00);
        FLhd_Data.push_back(0x00);
        for (uint8_t NUM : TO_BYTES_LITTLE_UINT8(Input_MIDI.getTrackCount(), 2))
        {
            FLhd_Data.push_back(NUM);
        }
        for (uint8_t NUM : TO_BYTES_LITTLE_UINT8(Input_MIDI.getTicksPerQuarterNote(), 2))
        {
            FLhd_Data.push_back(NUM);
        }
        vector<uint8_t> FLdt_Data;
        Make_FL_Event(FLdt_Data, 199, vector<uint8_t>{'8', '.', '0', '.', '0', 0});
        Make_FL_Event(FLdt_Data, 93, vector<uint8_t>{0x00});
        Make_FL_Event(FLdt_Data, 66, vector<uint8_t>{0xFF,0xFF});
        Make_FL_Event(FLdt_Data, 67, vector<uint8_t>{0x01});
        Make_FL_Event(FLdt_Data, 9, vector<uint8_t>{0x01});
        Make_FL_Event(FLdt_Data, 11, vector<uint8_t>{0x00});
        Make_FL_Event(FLdt_Data, 80, vector<uint8_t>{0x00});
        Make_FL_Event(FLdt_Data, 24, vector<uint8_t>{0x00});
        Make_FL_Event(FLdt_Data, 17, vector<uint8_t>{0x04});
        Make_FL_Event(FLdt_Data, 18, vector<uint8_t>{0x04});
        Make_FL_Event(FLdt_Data, 23, vector<uint8_t>{0x01});
        Make_FL_Event(FLdt_Data, 10, vector<uint8_t>{0x00});
        Make_FL_Event(FLdt_Data, 65, std::vector<uint8_t>{1});
        Make_FL_Event(FLdt_Data, 193, std::vector<uint8_t>{'m', 'i', 'd', 'i', '2', 'f', 'l', 'p', ' ', 'a', 'u', 't', 'o', ' ', 'g', 'e', 'n', 'e', 'r', 'a', 't', 'e', 'd', ' ', 'd', 'a', 't', 'a', 0});

        uint8_t ID_Plugin_New[24];
        for (uint8_t i = 0; i < 24; i++) ID_Plugin_New[i] = 0;
        ID_Plugin_New[8] = 2;
        ID_Plugin_New[16] = 16;
        uint8_t ID_Plugin_Parameters[384];
        for (uint16_t i = 0; i < 384; i++) ID_Plugin_Parameters[i] = 0;
        ID_Plugin_Parameters[0] = 6;
        for (uint8_t i = 8; i < 20; ++i) {
            ID_Plugin_Parameters[i] = 255;
        }
        ID_Plugin_Parameters[4] = 0; //MIDI output channel
        ID_Plugin_Parameters[29] = 1; //Map note color to midi channel
        ID_Plugin_Parameters[322] = 255;

        vector<FL_Event> MainNoteBin;

        parallel_for(0, Input_MIDI.getTrackCount(), [&](int TrackID) {
        //for (int TrackID = 0; TrackID < Input_MIDI.getTrackCount(); TrackID++){
            vector<FL_Event> TempNoteBin(ceil(static_cast<float>(Input_MIDI.getEventCount(TrackID)) / 2.0f));
            queue<MidiNote> NoteQueue[256 * 16];
            size_t NoteCount = 0;
            string TrackName = "MIDI Out";
            for (size_t EventIDX = 0; EventIDX < Input_MIDI.getEventCount(TrackID); EventIDX++) {
                MidiEvent Event = Input_MIDI[TrackID][EventIDX];
                if (Event.isNoteOn()) {
                    MidiNote Note;
                    Note.start = Event.tick;
                    Note.vol = Event.getVelocity();
                    NoteQueue[Event.getChannel() * 256 + Event.getKeyNumber()].push(Note);
                }
                else if (Event.isNoteOff()) {
                    if (!(NoteQueue[Event.getChannel() * 256 + Event.getKeyNumber()].empty())) {
                        MidiNote Note = NoteQueue[Event.getChannel() * 256 + Event.getKeyNumber()].front();
                        NoteQueue[Event.getChannel() * 256 + Event.getKeyNumber()].pop();
                        FL_Event& FL_Note = TempNoteBin[NoteCount];
                        FL_Note.pos = Note.start;
                        FL_Note.flags = 16384;
                        FL_Note.rack = TrackID;
                        FL_Note.dur = Event.tick - Note.start;
                        FL_Note.key = Event.getKeyNumber();
                        FL_Note.chan = Event.getChannel();
                        FL_Note.vol = Note.vol;
                        FL_Note.unk1 = 120;
                        FL_Note.unk2 = 0;
                        FL_Note.unk3 = 64;
                        FL_Note.unk4 = 128;
                        FL_Note.unk5 = 128;
                        NoteCount++;
                    }
                }
                else if (Event.isTrackName()) {
                    string Content = Event.getMetaContent();
                    if (Content.length() > 0) {
                        TrackName = Content;
                    }
                }
            }
            {
                lock_guard<mutex> lock(MutexLock);
                cout << "Converted Track: ";
                cout << TrackID+1;
                cout << "\n";
                MainNoteBin.insert(MainNoteBin.end(), TempNoteBin.begin(), TempNoteBin.begin() + NoteCount);
                Make_FL_Event(FLdt_Data, 64, TO_BYTES_LITTLE_UINT8(static_cast<uint16_t>(TrackID),2));
                Make_FL_Event(FLdt_Data, 21, vector<uint8_t>{0x02}); // Plugin type
                Make_FL_Event(FLdt_Data, 201, vector<uint8_t>{'M', 'I', 'D', 'I', ' ', 'O', 'u', 't', 0});
                Make_FL_Event(FLdt_Data, 212, vector<uint8_t>(ID_Plugin_New, ID_Plugin_New + 24));
                TrackName += ' ';
                TrackName += '#';
                TrackName += to_string(TrackID+1);
                TrackName += (char)0x00;
                Make_FL_Event(FLdt_Data, 192, vector<uint8_t>(TrackName.begin(), TrackName.end()));
                Make_FL_Event(FLdt_Data, 213, vector<uint8_t>(ID_Plugin_Parameters, ID_Plugin_Parameters + 384));
            }
        });

        cout << "Sorting events... \n";
        sort(MainNoteBin.begin(), MainNoteBin.end(), [](FL_Event& NoteA, FL_Event& NoteB) {return NoteA.pos < NoteB.pos; });
        size_t ByteSize = MainNoteBin.size() * sizeof(FL_Event);
        vector<uint8_t> BytesNoteBin(ByteSize);
        memcpy(BytesNoteBin.data(), MainNoteBin.data(), ByteSize);

        cout << "Writing FLP... \n";
        Make_FL_Event(FLdt_Data, 224, BytesNoteBin);
        Make_FL_Event(FLdt_Data, 129, TO_BYTES_LITTLE_UINT8(65536, sizeof(uint32_t)));

        Output_FLP.write("FLhd", 4);
        uint32_t data_FLhd_size = FLhd_Data.size();
        Output_FLP.write(reinterpret_cast<char*>(&data_FLhd_size), 4);
        Output_FLP.write(reinterpret_cast<char*>(FLhd_Data.data()), FLhd_Data.size());

        Output_FLP.write("FLdt", 4);
        uint32_t data_FLdt_size = FLdt_Data.size();
        Output_FLP.write(reinterpret_cast<char*>(&data_FLdt_size), 4);
        Output_FLP.write(reinterpret_cast<char*>(FLdt_Data.data()), FLdt_Data.size());
    }
    return 0;
}
