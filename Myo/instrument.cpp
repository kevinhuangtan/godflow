// Copyright (C) 2013-2014 Thalmic Labs Inc.
// Distributed under the Myo SDK license agreement. See LICENSE.txt for details.
#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <stdio.h>

/********** FOR MIDI *********/
#include <iostream>
#include <cstdlib>
#include <ncurses.h>
#include "RtMidi.h"

using namespace std;

bool chooseMidiPort( RtMidiOut *rtmidi );
void play_note(RtMidiOut *midiout,  int note, vector<unsigned char>& message);
void stop_note( RtMidiOut *midiout, int note, vector<unsigned char>& message);
void pitch_bend(RtMidiOut *midiout, int cc, vector<unsigned char>& message);
void volume_change(RtMidiOut *midiout, int cc, vector<unsigned char>& message);

int startingPitch = 50;
int currentPitch = 0;
RtMidiOut *midiout = 0;
std::vector<unsigned char> message;

int intervals [8] = {0, 1, 2, 2, 3, 4, 5, 5};

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
        myo->unlock(myo::Myo::unlockHold);

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
        pitch_w = static_cast<int>((pitch + (float)M_PI/2.0f)/M_PI * 8);
        yaw_w = static_cast<int>((yaw + (float)M_PI)/(M_PI * 2.0f) * 127);

        if (identifyMyo(myo) == leftMyo){
            if ((currentPitch != (pitch_w + startingPitch)) && (currentPoseRight == myo::Pose::fist)) {
                stop_note(midiout, currentPitch, message);
                currentPitch = pitch_w + startingPitch + intervals[pitch_w];
                play_note(midiout, currentPitch, message);
            }
            pitch_bend(midiout, roll_w, message);
        } else if (identifyMyo(myo) == rightMyo) {
            volume_change(midiout, yaw_w, message);
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

        // Vibrate the Myo whenever we've detected that the user has made a fist.
        // if (pose == myo::Pose::fist) {
        //     myo->vibrate(myo::Myo::vibrationMedium);
        // }

        // if (pose != myo::Pose::unknown && pose != myo::Pose::rest) {
        //     // Tell the Myo to stay unlocked until told otherwise. We do that here so you can hold the poses without the
        //     // Myo becoming locked.
        //     myo->unlock(myo::Myo::unlockHold);

        //     // Notify the Myo that the pose has resulted in an action, in this case changing
        //     // the text on the screen. The Myo will vibrate.
        //     myo->notifyUserAction();
        // } else {
        //     // Tell the Myo to stay unlocked only for a short period. This allows the Myo to stay unlocked while poses
        //     // are being performed, but lock after inactivity.
        //     myo->unlock(myo::Myo::unlockTimed);
        // }

        // if (currentPoseLeft == myo::Pose::waveOut) {
        //   myo->vibrate(myo::Myo::vibrationMedium);
        //   startingPitch += 12;
        // }

        // if (currentPoseLeft == myo::Pose::waveIn) {
        //   myo->vibrate(myo::Myo::vibrationMedium);
        //   startingPitch -= 12;
        // }

        int midiNote = pitch_w + startingPitch;
        std::cout << "New Val: " << midiNote << std::endl;
        if (currentPoseRight == myo::Pose::fist) {
            stop_note(midiout, currentPitch, message);
            play_note(midiout, currentPitch, message);
        }

        if ((currentPoseRight == myo::Pose::fingersSpread) || (currentPoseRight == myo::Pose::rest)) {
            stop_note(midiout, currentPitch, message);
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
        std::cout << "Left Myo: " << leftMyo << std::endl;
    }

    // onArmUnsync() is called whenever Myo has detected that it was moved from a stable position on a person's arm after
    // it recognized the arm. Typically this happens when someone takes Myo off of their arm, but it can also happen
    // when Myo is moved around on the arm.
    void onArmUnsync(myo::Myo* myo, uint64_t timestamp)
    {
        onArm = false;
    }

    // There are other virtual functions in DeviceListener that we could override here, like onAccelerometerData().
    // For this example, the functions overridden above are sufficient.

    // We define this function to print the current values that were updated by the on...() functions above.
    
    // onUnlock() is called whenever Myo has become unlocked, and will start delivering pose events.
    void onUnlock(myo::Myo* myo, uint64_t timestamp)
    {
        isUnlocked = true;
    }

    // onLock() is called whenever Myo has become locked. No pose events will be sent until the Myo is unlocked again.
    void onLock(myo::Myo* myo, uint64_t timestamp)
    {
        isUnlocked = false;
    }

    void onOrientationData(myo::Myo* myo, uint64_t timestamp) {
        std::cout << 'Meow' << std::endl;
    }

    void print()
    {
        // Clear the current line
        std::cout << '\r';

        // Print out the orientation. Orientation data is always available, even if no arm is currently recognized.
        // std::cout << '[' << std::string(roll_w, '*') << std::string(12 - roll_w, ' ') << ']'
        //           << '[' << std::string(pitch_w, '*') << std::string(12 - pitch_w, ' ') << ']'
        //           << '[' << std::string(yaw_w, '*') << std::string(12 - yaw_w, ' ') << ']';

        std::cout << pitch_w;

        if (onArm) {
            // Print out the currently recognized pose and which arm Myo is being worn on.

            // Pose::toString() provides the human-readable name of a pose. We can also output a Pose directly to an
            // output stream (e.g. std::cout << currentPose;). In this case we want to get the pose name's length so
            // that we can fill the rest of the field with spaces below, so we obtain it as a string using toString().
            // std::string poseString = currentPose.toString();


            // std::cout << '[' << (whichArm == myo::d ? "L" : "R") << ']'
            //           << '[' << poseString << std::string(14 - poseString.size(), ' ') << ']';
        } else {
            // Print out a placeholder for the arm and pose when Myo doesn't currently know which arm it's on.
            // std::cout << "[?]" << '[' << std::string(14, ' ') << ']';
        }

        std::cout << std::flush;
    }

    void onConnect(myo::Myo* myo, uint64_t timestamp, myo::FirmwareVersion firmwareVersion)
    {
        std::cout << "Myo " << identifyMyo(myo) << " has connected." << std::endl;
    }

    void onDisconnect(myo::Myo* myo, uint64_t timestamp)
    {
        std::cout << "Myo " << identifyMyo(myo) << " has disconnected." << std::endl;
    }

    // This is a utility function implemented for this sample that maps a myo::Myo* to a unique ID starting at 1.
    // It does so by looking for the Myo pointer in knownMyos, which onPair() adds each Myo into as it is paired.
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

    // These values are set by onArmSync() and onArmUnsync() above.
    bool onArm;
    myo::Arm whichArm;

    bool isUnlocked;
    int leftMyo;
    int rightMyo;

    // We store each Myo pointer that we pair with in this list, so that we can keep track of the order we've seen
    // each Myo and give it a unique short identifier (see onPair() and identifyMyo() above).
    std::vector<myo::Myo*> knownMyos;

    // These values are set by onOrientationData() and onPose() above.
    int roll_w, pitch_w, yaw_w;
    // myo::Pose currentPose;
    myo::Pose currentPoseRight;
    myo::Pose currentPoseLeft;
};

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

    message.push_back( 192 );
    message.push_back( 5 );
    midiout->sendMessage( &message );
    message.push_back( 100 );



    // We catch any exceptions that might occur below -- see the catch statement for more details.
    try {

    // First, we create a Hub with our application identifier. Be sure not to use the com.example namespace when
    // publishing your application. The Hub provides access to one or more Myos.
    myo::Hub hub("com.example.hello-myo");

    std::cout << "Attempting to find a Myo..." << std::endl;

    // Next, we attempt to find a Myo to use. If a Myo is already paired in Myo Connect, this will return that Myo
    // immediately.
    // waitForAnyMyo() takes a timeout value in milliseconds. In this case we will try to find a Myo for 10 seconds, and
    // if that fails, the function will return a null pointer.
    // myo::Myo* myo = hub.waitForMyo(10000);

    // If waitForAnyMyo() returned a null pointer, we failed to find a Myo, so exit with an error message.
    // if (!myo) {
    //     throw std::runtime_error("Unable to find a Myo!");
    // }

    // We've found a Myo.
    // std::cout << "Connected to a Myo armband!" << std::endl << std::endl;

    // Next we construct an instance of our DeviceListener, so that we can register it with the Hub.
    DataCollector collector;

    // Hub::addListener() takes the address of any object whose class inherits from DeviceListener, and will cause
    // Hub::run() to send events to all registered device listeners.
    hub.addListener(&collector);

    // Finally we enter our main loop.
    while (1) {
        // In each iteration of our main loop, we run the Myo event loop for a set number of milliseconds.
        // In this case, we wish to update our display 20 times a second, so we run for 1000/20 milliseconds.
        hub.run(1);
        // for (int i = 30; i < 91; i++) {
        
        // }
        // std::cout << collector.pitch_w << " : " << identifyMyo(myo) << std::endl << std::endl;
        // After processing events, we call the print() member function we defined above to print out the values we've
        // obtained from any events that have occurred.
        // collector.print();
    }

    // If a standard exception occurred, we print out its message and exit.
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
    message[2] = 127;
    midiout->sendMessage( &message );
}

void stop_note( RtMidiOut *midiout, int note, vector<unsigned char>& message) {
    // Note Off: 128, 64, 40
    message[0] = 128;
    message[1] = note;
    message[2] = 127;
    midiout->sendMessage( &message );
}

void pitch_bend(RtMidiOut *midiout, int cc, vector<unsigned char>& message) {
    // cout << "NOTE: " << cc << endl;
    // cout << "SENDING PITCH BEND" << endl;
    // cc = (cc)/127 * (74-54) + 54;
    // cout << "CC " << cc << endl;
    message[0] = 176;
    message[1] = 9;
    message[2] = cc;
    midiout->sendMessage( &message );
}

void volume_change(RtMidiOut *midiout, int cc, vector<unsigned char>& message) {
    // note = (note)/127 * (74-54) + 54;
    cc = cc + 90;
    // if (cc > 127) {
    //     cc = 127;
    // }
    // cout << "PIZZA: " << cc << endl;
    message[0] = 176;
    message[1] = 8;
    message[2] = cc;
    midiout->sendMessage( &message );
}