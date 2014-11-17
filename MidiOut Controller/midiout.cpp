//*****************************************//
//  midiout.cpp
//
//  mapping Myo to Ableton
//
//*****************************************//

#include <iostream>
#include <cstdlib>
#include <ncurses.h>
#include "RtMidi.h"

using namespace std;

// Platform-dependent sleep routines.
#if defined(__WINDOWS_MM__)
  #include <windows.h>
  #define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
  #include <unistd.h>
  #define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif

// This function should be embedded in a try/catch block in case of
// an exception.  It offers the user a choice of MIDI ports to open.
// It returns false if there are no ports available.
bool chooseMidiPort( RtMidiOut *rtmidi );
void play_note(RtMidiOut *midiout,  int note, vector<unsigned char>& message) ;
void control_change_1( RtMidiOut *midiout, int cc, vector<unsigned char>& message);
void control_change_2( RtMidiOut *midiout, int cc, vector<unsigned char>& message);

int main( void )
{
  RtMidiOut *midiout = 0;
  std::vector<unsigned char> message;

  // RtMidiOut constructor
  try {
    midiout = new RtMidiOut();
  }
  catch ( RtMidiError &error ) {
    error.printMessage();
    exit( EXIT_FAILURE );
  }

  // Call function to select port.
  try {
    if ( chooseMidiPort( midiout ) == false ) goto cleanup;
  }
  catch ( RtMidiError &error ) {
    error.printMessage();
    goto cleanup;
  }

  // Send out a series of MIDI messages.

  // Program change: 192, 5
  message.push_back( 192 );
  message.push_back( 5 );
  midiout->sendMessage( &message );
  message.push_back( 100 );
  
  cout<<"use left and right arrow keys for keys"<<endl<<"use a and d for continuous controller 1"endl<<"use z and c for continuous controller 2"<<endl<<"press q to quit";
  SLEEP(3000);
  //set up continuous key input
  initscr();
  noecho();
  keypad(stdscr,TRUE);


  //parameters
  int key;
  key = 60;

  int cc_1;
  cc_1 = 100;

  int cc_2;
  cc_2 = 100;


  int volume;
  volume = 60;


  while(true){
    SLEEP(20);
    int ch;
    ch = getch();
    cout<<ch;

    //play notes
    if(ch == KEY_LEFT && key > 1){
        key = key-1;
        cout<<key;
        // cout<<volume;
        play_note(midiout, key, message);
    }
    if(ch == KEY_RIGHT && key <127){
        key = key + 1;
        cout<<key;
            // Note On: 144, 64, 90
         play_note(midiout, key, message);
    }

    //change CC #1
    //a
    if(ch == 97 && cc_1 > 1){
        cc_1 = cc_1 - 1;
        cout<<cc_1;
            // Note On: 144, 64, 90
        control_change_1(midiout, cc_1, message);
    }
    //d
    if(ch == 100 && cc_1 <127){
        cc_1 = cc_1 + 1;
        cout<<cc_1;
            // Note On: 144, 64, 90
        control_change_1(midiout, cc_1, message);
    }
    //change CC #2
    //z
    if(ch == 122 && cc_2 > 1){
        cc_2 = cc_2 - 1;
        cout<<cc_2;
            // Note On: 144, 64, 90
        control_change_2(midiout, cc_2, message);
    }
    //c
    if(ch == 99 && cc_2 < 127){
        cc_2 = cc_2 + 1;
        cout<<cc_2;
            // Note On: 144, 64, 90
        control_change_2(midiout, cc_2, message);
    }

    //quit q
    if(ch == 113){
      break;
    }
    refresh(); 


    }
 endwin();  

  // Clean up
 cleanup:
  delete midiout;

  return 0;
}

bool chooseMidiPort( RtMidiOut *rtmidi )
{

  rtmidi->openVirtualPort();
  return true;
}

void play_note( RtMidiOut *midiout, int note, vector<unsigned char>& message)
{
// Note On: 144, 64, 90
  message[0]=144;
  message[1]=note;
  message[2]=80;
  midiout->sendMessage( &message );

  SLEEP( 50 ); 
  // Note Off: 128, 64, 40
  message[0] = 128;
  message[1] = note;
  message[2] = 80;
  midiout->sendMessage( &message );

}

void control_change_1( RtMidiOut *midiout, int cc, vector<unsigned char>& message)
{
  //declare cc
  message[0]=176;
  //cc channel
  message[1]=7;
  message[2]=cc;
  midiout->sendMessage( &message );
}

void control_change_2( RtMidiOut *midiout, int cc, vector<unsigned char>& message)
{
  //declare cc
  message[0]=176;
  //cc channel
  message[1]=1;
  message[2]=cc;
  midiout->sendMessage( &message );
}
