
#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <algorithm>

/********** FOR MIDI *********/
#include <iostream>
#include <cstdlib>
#include <ncurses.h>
#include "RtMidi.h"

using namespace std;

bool chooseMidiPort( RtMidiOut *rtmidi );
// bool chooseMidiPortIn( RtMidiIn *rtmidi );
void play_note(RtMidiOut *midiout,  int note, vector<unsigned char>& message);
void drum(RtMidiOut *midiout,  int note, vector<unsigned char>& message);
void play( RtMidiOut *midiout, int row, vector<unsigned char>& message);
void pause( RtMidiOut *midiout,vector<unsigned char>& message);
void beat_repeat(RtMidiOut *midiout,vector<unsigned char>& message);
void beat_on(RtMidiOut *midiout,vector<unsigned char>& message);
void offset(RtMidiOut *midiout, int cc, vector<unsigned char>& message);
void control_change_1( RtMidiOut *midiout, int cc, vector<unsigned char>& message);
void control_change_2( RtMidiOut *midiout, int cc, vector<unsigned char>& message);
int ableton_current_row = 1;
bool playing_clip = false; 
int drum_note_roll = 60;
int drum_note_pitch = 60;
int drum_note_yaw = 60;
RtMidiOut *midiout = 0;
std::vector<unsigned char> message;

// Platform-dependent sleep routines.
#if defined(__WINDOWS_MM__)
  #include <windows.h>
  #define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
  #include <unistd.h>
  #define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif

/******************************/

// The only file that needs to be included to use the Myo C++ SDK is myo.hpp.
#include <myo/myo.hpp>

// Classes that inherit from myo::DeviceListener can be used to receive events from Myo devices. DeviceListener
// provides several virtual functions for handling different kinds of events. If you do not override an event, the
// default behavior is to do nothing.
class DataCollector : public myo::DeviceListener {
public:
    DataCollector()
    : onArm(false), isUnlocked(false), roll_w(0), pitch_w(0), yaw_w(0), currentPoseRight(), currentPoseLeft()
    {
    }
    void onPair(myo::Myo* myo, uint64_t timestamp, myo::FirmwareVersion firmwareVersion)
    {
        // Print out the MAC address of the armband we paired with.

        // The pointer address we get for a Myo is unique - in other words, it's safe to compare two Myo pointers to
        // see if they're referring to the same Myo.

        // Add the Myo pointer to our list of known Myo devices. This list is used to implement identifyMyo() below so
        // that we can give each Myo a nice short identifier.
        
        knownMyos.push_back(myo);

        // Now that we've added it to our list, get our short ID for it and print it out.
        std::cout << "Paired with " << identifyMyo(myo) << "." << std::endl;
        
    }

    // onUnpair() is called whenever the Myo is disconnected from Myo Connect by the user.
    void onUnpair(myo::Myo* myo, uint64_t timestamp)
    {
        // We've lost a Myo.
        // Let's clean up some leftover state.
        roll_w = 0;
        pitch_w = 0;
        yaw_w = 0;
        onArm = false;
        isUnlocked = false;
    }

    // onOrientationData() is called whenever the Myo device provides its current orientation, which is represented
    // as a unit quaternion.
    void onOrientationData(myo::Myo* myo, uint64_t timestamp, const myo::Quaternion<float>& quat)
    {
        using std::atan2;
        using std::asin;
        using std::sqrt;
        using std::max;
        using std::min;

        // Calculate Euler angles (roll, pitch, and yaw) from the unit quaternion.
        float roll = atan2(2.0f * (quat.w() * quat.x() + quat.y() * quat.z()),
                           1.0f - 2.0f * (quat.x() * quat.x() + quat.y() * quat.y()));
        float pitch = asin(max(-1.0f, min(1.0f, 2.0f * (quat.w() * quat.y() - quat.z() * quat.x()))));
        float yaw = atan2(2.0f * (quat.w() * quat.z() + quat.x() * quat.y()),
                        1.0f - 2.0f * (quat.y() * quat.y() + quat.z() * quat.z()));

        // Convert the floating point angles in radians to a scale from 0 to 18.
        roll_w = static_cast<int>((roll + (float)M_PI)/(M_PI * 2.0f) * 127);
        pitch_w = static_cast<int>((pitch + (float)M_PI/2.0f)/M_PI * 127);
        yaw_w = static_cast<int>((yaw + (float)M_PI)/(M_PI * 2.0f) * 127);

        if (identifyMyo(myo) == rightMyo) {
          if (currentPoseRight == myo::Pose::fist && isUnlocked ==true){
             control_change_1(midiout, roll_w, message);
             control_change_2(midiout, pitch_w, message);
          }
          if (isUnlocked ==true && pitch_w < 20){
             std::cout<<pitch_w;
             play(midiout, ableton_current_row, message);
          }
        } else if (identifyMyo(myo) == leftMyo) {
          if (currentPoseLeft == myo::Pose::fist && isUnlocked ==true){
            offset(midiout, roll_w, message);
          }
          if (isUnlocked ==true){
             if (pitch_w>80){
                if (drum_note_pitch!=61){
                  drum_note_pitch = 61;
                  drum(midiout, drum_note_pitch, message);
                  std::cout<<"pitch";
                }      
             }
             if (pitch_w<40){
                if (drum_note_pitch!=62){
                  drum_note_pitch = 62;
                  drum(midiout, drum_note_pitch, message);
                  std::cout<<"pitch";
                }      
             }
             if (roll_w>75){
                if (drum_note_roll!=63){
                  drum_note_roll = 63;
                  drum(midiout, drum_note_roll, message);
                  std::cout<<"roll";
                }      
             }
             // if (roll_w<70){
             //    if (drum_note_roll!=64){
             //      drum_note_roll = 64;
             //      drum(midiout, drum_note_roll, message);
             //      std::cout<<"roll";
             //    }      
             // }
             // if (yaw_w>20&&yaw_w<70){
             //    if (drum_note_yaw!=65){
             //      drum_note_yaw = 65;
             //      drum(midiout, drum_note_yaw, message);
             //      std::cout<<"yaw";
             //      std::cout<<yaw_w;
             //    }      
             // }
             // if (yaw_w<10){
             //    if (drum_note_yaw!=66){
             //      drum_note_yaw = 66;
             //      drum(midiout, drum_note_yaw, message);
             //      std::cout<<"yaw";
             //      std::cout<<yaw_w;
             //    }      
             // }
                
          }
        }

    }

    // onPose() is called whenever the Myo detects that the person wearing it has changed their pose, for example,
    // making a fist, or not making a fist anymore.
    void onPose(myo::Myo* myo, uint64_t timestamp, myo::Pose pose)
    {

        if (identifyMyo(myo) == rightMyo) {
            currentPoseRight = pose;
        } else {
            currentPoseLeft = pose;
        }

        // if (identifyMyo(myo))
        // Vibrate the Myo whenever we've detected that the user has made a fist.
        // if (pose == myo::Pose::fist) {
        //     myo->vibrate(myo::Myo::vibrationMedium);
        // }

      

        if (currentPoseRight == myo::Pose::waveIn) {
          if (ableton_current_row > 1){
            ableton_current_row--;
            play(midiout, ableton_current_row, message);
          }
        }

        if (currentPoseRight == myo::Pose::waveOut) {
          if (ableton_current_row < 7){
            ableton_current_row++;
            play(midiout, ableton_current_row, message);
          }        
        }

        if (currentPoseRight== myo::Pose::fingersSpread){
          if (playing_clip == false){
            play(midiout, ableton_current_row, message);
            playing_clip = true;
          }
          else{
            if(playing_clip==true){
              pause(midiout, message);
              playing_clip = false;
            }
          }
        }

        if (currentPoseLeft == myo::Pose::doubleTap) {
          beat_on(midiout, message);
        }

        if (currentPoseLeft == myo::Pose::waveOut) {
          beat_repeat(midiout, message);
        }

    }

    // onArmSync() is called whenever Myo has recognized a Sync Gesture after someone has put it on their
    // arm. This lets Myo know which arm it's on and which way it's facing.
    void onArmSync(myo::Myo* myo, uint64_t timestamp, myo::Arm arm, myo::XDirection xDirection)
    {
        onArm = true;
        whichArm = arm;

        if (arm == myo::armLeft) {
            leftMyo = identifyMyo(myo);
        }
        if (arm == myo::armRight){
            rightMyo = identifyMyo(myo);
        }
    }

    // onArmUnsync() is called whenever Myo has detected that it was moved from a stable position on a person's arm after
    // it recognized the arm. Typically this happens when someone takes Myo off of their arm, but it can also happen
    // when Myo is moved around on the arm.
    void onArmUnsync(myo::Myo* myo, uint64_t timestamp)
    {
        onArm = false;
    }

    void onUnlock(myo::Myo* myo, uint64_t timestamp)
    {
        isUnlocked = true;
    }

    // onLock() is called whenever Myo has become locked. No pose events will be sent until the Myo is unlocked again.
    void onLock(myo::Myo* myo, uint64_t timestamp)
    {
        isUnlocked = false;
    }

    void print()
    {
        // Clear the current line
        std::cout << '\r';

        std::cout<<"roll: "<<roll_w<<" ";
        std::cout<<"pitch: "<<pitch_w<<" ";
        std::cout<<"yaw: "<<yaw_w<<" ";

        std::cout << std::flush;
    }
    void onConnect(myo::Myo* myo, uint64_t timestamp, myo::FirmwareVersion firmwareVersion)
    {
        std::cout << "Myo " << identifyMyo(myo) << " has connected." << std::endl;
        myo->unlock(myo::Myo::unlockHold);
    }
    void onDisconnect(myo::Myo* myo, uint64_t timestamp)
    {
        std::cout << "Myo " << identifyMyo(myo) << " has disconnected." << std::endl;
    }
    size_t identifyMyo(myo::Myo* myo) {
        // Walk through the list of Myo devices that we've seen pairing events for.
        for (size_t i = 0; i < knownMyos.size(); ++i) {
            // If two Myo pointers compare equal, they refer to the same Myo device.
            if (knownMyos[i] == myo) {
                return i + 1;
            }
        }

        return 0;
    }
    std::vector<myo::Myo*> knownMyos;

    // These values are set by onArmSync() and onArmUnsync() above.
    bool onArm;
    myo::Arm whichArm;

    bool isUnlocked;
    int leftMyo;
    int rightMyo;
    // These values are set by onOrientationData() and onPose() above.
    int roll_w, pitch_w, yaw_w;
    myo::Pose currentPoseRight;
    myo::Pose currentPoseLeft;
};



//#######################main loop#############/
int main(int argc, char** argv)
{


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
        if ( chooseMidiPort( midiout ) == false ){
            delete midiout;
            return 0;
        }
      }
      catch ( RtMidiError &error ) {
        error.printMessage();
        delete midiout;
        return 0;
      }


    //initialize midi vector
    message.push_back( 192 );
    message.push_back( 5 );
    message.push_back( 100 );

    try {

    // First, we create a Hub with our application identifier. Be sure not to use the com.example namespace when
    // publishing your application. The Hub provides access to one or more Myos.
    myo::Hub hub("com.example.multiple-myos");

    std::cout << "Attempting to find a Myo..." << std::endl;

    // Next, we attempt to find a Myo to use. If a Myo is already paired in Myo Connect, this will return that Myo
    // immediately.
    myo::Myo* myo = hub.waitForMyo(10000);

    if (!myo) {
        throw std::runtime_error("Unable to find a Myo!");
    }
    std::cout << "Connected to a Myo armband!" << std::endl << std::endl;



    // Next we construct an instance of our DeviceListener, so that we can register it with the Hub.
    DataCollector collector;

    hub.addListener(&collector);

    // MAIN LOOP
    
    while (1) {

        hub.run(50);

        collector.print();
    }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Press enter to continue.";
        std::cin.ignore();
        return 1;
    }

}

bool chooseMidiPort( RtMidiOut *rtmidi )
{
    rtmidi->openVirtualPort();
    return true;
}

void play_note( RtMidiOut *midiout, int note, vector<unsigned char>& message)
{
// Note On: 144, 64, 90
  message[0] = 144;
  message[1] = note;
  message[2] = 80;
  midiout->sendMessage( &message );

  SLEEP( 50 ); 
  // Note Off: 128, 64, 40
  message[0] = 128;
  message[1] = note;
  message[2] = 80;
  midiout->sendMessage( &message );

}
void play( RtMidiOut *midiout, int row, vector<unsigned char>& message)
{
        std::cout << '\n';
        std::cout<<"play";
        std::cout << std::flush;

  //channel 1, first clip
  message[0] = 144;
  message[1] = 53 - 1 + row;
  message[2] = 127;
  midiout->sendMessage( &message );

  message[0] = 128;
  message[1] = 53 - 1 + row;
  message[2] = 127;
  midiout->sendMessage( &message );
}

void pause( RtMidiOut *midiout, vector<unsigned char>& message)
{

  std::cout << '\n';
  std::cout<<"pause";
  std::cout << std::flush;
  //channel 1, first clip
  message[0] = 144;
  message[1] = 52;
  message[2] = 127;
  midiout->sendMessage( &message );

  SLEEP( 50 ); 
  // Note Off: 128, 64, 40
  message[0] = 128;
  message[1] = 52;
  message[2] = 127;
  midiout->sendMessage( &message );
}

void control_change_1( RtMidiOut *midiout, int cc, vector<unsigned char>& message)
{
  //declare cc
  cc = cc + 50;
  message[0]=176;
  //cc channel
  message[1]=7;
  message[2]=(int) (5 * (double) cc);

  midiout->sendMessage( &message );
}
void control_change_2( RtMidiOut *midiout, int cc, vector<unsigned char>& message)
{
  cc = cc - 30;
  //declare cc
  message[0]=176;
  //cc channel
  message[1]=8;
  message[2]=128 -(int) (3 * (double) cc);

  midiout->sendMessage( &message );
}
void beat_repeat(RtMidiOut *midiout, vector<unsigned char>& message){
  std::cout << '\n';
  std::cout<<"beat repeat";
  std::cout << std::flush;

  message[0] = 144;
  message[1] = 51;
  message[2] = 127;
  midiout->sendMessage( &message );

  SLEEP( 50 ); 
  message[0] = 128;
  message[1] = 51;
  message[2] = 127;
  midiout->sendMessage( &message );
}
void beat_on(RtMidiOut *midiout, vector<unsigned char>& message){
  std::cout << '\n';
  std::cout<<"beat on";
  std::cout << std::flush;

  message[0] = 144;
  message[1] = 50;
  message[2] = 127;
  midiout->sendMessage( &message );

  SLEEP( 50 ); 
  message[0] = 128;
  message[1] = 50;
  message[2] = 127;
  midiout->sendMessage( &message );
}
void offset(RtMidiOut *midiout, int cc, vector<unsigned char>& message){
 
  cc = cc + 40;
  message[0]=176;
  //cc channel
  message[1]=9;
  message[2]=(int) (3 * (double) cc);

  midiout->sendMessage( &message );
}
void drum(RtMidiOut *midiout, int note, vector<unsigned char>& message){

  message[0]=145;
  //cc channel
  message[1]=note-20;
  message[2]=100;

  midiout->sendMessage( &message );
  SLEEP( 50 ); 
  // Note Off: 128, 64, 40
  message[0] = 129;
  message[1] = note-30;
  message[2] = 100;
  midiout->sendMessage( &message );
  std::cout<<note;

}
