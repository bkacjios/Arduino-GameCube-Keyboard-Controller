#include <Nintendo.h>
#include <Gamecube_Keyboard.h>

#include <Joystick.h>

#include <Keyboard.h>
#include <KeyboardLayout.h>

//#define DEBUG

#define NUM_PORTS 2
#define NUM_BUTTONS 8
#define NUM_DPADS 1

#define PORT_1 0
#define PORT_2 1

// Our controller ports are wired to pin 4 and 7
CGamecubeController* gc_controllers[2] = {
  new CGamecubeController(4),
  new CGamecubeController(7)
};

// What port we are currently going to poll from.
volatile byte PORT = PORT_1;

// The last known device of each port.
volatile int gc_device_ids[NUM_PORTS] = {NINTENDO_DEVICE_GC_NONE, NINTENDO_DEVICE_GC_NONE};

// Tracks if the USB keyboard is pressing a key or not.
volatile boolean gc_keyboard_pressed[GCKEY_MAX] = {};

// The previous reported keyboard state of a gamecube keyboard.
volatile byte gc_keyboard_last_press[3][NUM_PORTS] = {};

// Setup interfaces for USB joysticks for all our ports.
// We start at device ID 0x03 since 0x01 and 0x02 are for the Keyboard/Mouse HID libraries.

Joystick_* joysticks[NUM_PORTS] = {
 new Joystick_(
  0x03, JOYSTICK_TYPE_GAMEPAD,
  NUM_BUTTONS, NUM_DPADS, // Button Count, Hat Switch Count
  true, true, true,       // X, Y, and L analog axis
  true, true, true,       // cX, cY, and R analog axis
  false, false,           // No rudder or throttle
  false, false, false     // No accelerator, brake, or steering
  ),
 new Joystick_(
  0x04, JOYSTICK_TYPE_GAMEPAD,
  NUM_BUTTONS, NUM_DPADS, // Button Count, Hat Switch Count
  true, true, true,       // X, Y, and L analog axis
  true, true, true,       // cX, cY, and R analog axis
  false, false,           // No rudder or throttle
  false, false, false     // No accelerator, brake, or steering
  ),
};

void setup() {
#ifdef DEBUG
  while (!Serial) Serial.begin(115200);
#endif

  Keyboard.begin(KeyboardLayout_en_US);

  for (byte i=PORT_1; i<PORT2; i++) {
    auto joystick = joysticks[i];

    // Set all ranges to 0-255
    joystick->setXAxisRange(0, 255);
    joystick->setYAxisRange(0, 255);
    joystick->setRxAxisRange(0, 255);
    joystick->setRyAxisRange(0, 255);
    joystick->setZAxisRange(0, 255);
    joystick->setRzAxisRange(0, 255);
  
    // Initialize our joystick.
    // false = disable automatic reporting.
    joystick->begin(false);
  }
}

// the loop function runs over and over again forever
void loop() {
  // Poll the port
  poll(PORT);

  // Alternate polling between port 1 and port 2  
  PORT = !PORT;
}

void poll(byte port) {
    // Get the controller object we want to poll
  auto controller = gc_controllers[port];

  // Get the last known connected device for this port.
  int device = gc_device_ids[port];

  // If valid read..
  if (controller->read()) {
    // Get controller information
    auto status = controller->getStatus();

    // If this port was marked as being unoccupied..
    if (device == NINTENDO_DEVICE_GC_NONE) {
#ifdef DEBUG
      Serial.print("Port ");
      Serial.print(port+1);
      Serial.print(" connected : ");
      Serial.println(status.device, HEX);
#endif
      // Mark the port with our device type..
      gc_device_ids[port] = status.device;
    }

    // Get the report data
    auto report = controller->getReport();

    // Report the controller or keyboard..
    switch (status.device) {
      case NINTENDO_DEVICE_GC_WIRED:
        controller_report(port, report);
        break;
      case NINTENDO_DEVICE_GC_KEYBOARD:
        keyboard_report(port, report);
        break;
      default:
        break;
    }
  } else if(device != NINTENDO_DEVICE_GC_NONE) { // If we failed to read and we had a device on this port..

    // Lookup what device id was last known to be connected.
    int device = gc_device_ids[port];
    
#ifdef DEBUG
    Serial.print("Port ");
    Serial.print(port+1);
    Serial.print(" disconnect: ");
    Serial.println(device, HEX);
#endif

    // Mark this port as having no device.
    gc_device_ids[port] = NINTENDO_DEVICE_GC_NONE;
  }
}

void controller_report(byte port, Gamecube_Report_t &gc_report) {
  // Get the joystick HID for this port.
  auto joystick = joysticks[port];

  // Set all our buttons.
  joystick->setButton(0, gc_report.a);
  joystick->setButton(1, gc_report.b);
  joystick->setButton(2, gc_report.x);
  joystick->setButton(3, gc_report.y);
  joystick->setButton(4, gc_report.z);
  joystick->setButton(5, gc_report.l);
  joystick->setButton(6, gc_report.r);
  joystick->setButton(7, gc_report.start);
  
  // Set Main-Stick analog values.
  joystick->setXAxis(gc_report.xAxis);
  joystick->setYAxis(255 - gc_report.yAxis); // Y axis is inverted

  // Set C-Stick analog values.
  joystick->setRxAxis(gc_report.cxAxis);
  joystick->setRyAxis(255 - gc_report.cyAxis); // Y axis is inverted

  // Set L/R analog values.
  joystick->setZAxis(gc_report.left);
  joystick->setRzAxis(gc_report.right);

  // Default our hat to released.
  int16_t angle = JOYSTICK_HATSWITCH_RELEASE;

  // Convert dpad to an angle if any dpad buttons are pressed.
  if (gc_report.dup && gc_report.dright) {
    angle = 45; // North-East
  } else if (gc_report.ddown && gc_report.dright) {
    angle = 135; // South-East
  } else if (gc_report.ddown && gc_report.dleft) {
    angle = 225; // South-West
  } else if (gc_report.dup && gc_report.dleft) {
    angle = 315; // North-West
  } else if (gc_report.dup) {
    angle = 0; // North
  } else if (gc_report.dright) {
    angle = 90; // East
  } else if (gc_report.ddown) {
    angle = 180; // South
  } else if (gc_report.dleft) {
    angle = 270; // West
  }

  // Set our hat angle
  joystick->setHatSwitch(0, angle);

  // Send state to USB, since we disabled automatic reporting.
  joystick->sendState();
}

bool is_pressed(Gamecube_Report_t &gc_report, byte key) {
  // Loop through the reported key presses..

  if (gc_report.keyboard.keypress[0] == key) return true;
  if (gc_report.keyboard.keypress[1] == key) return true;
  if (gc_report.keyboard.keypress[2] == key) return true;
  
//  for (byte i=0; i<3; i++) {
//    // Return true if the key is being pressed in the report.
//    if (gc_report.keyboard.keypress[i] == key) return true;
//  }
  // Welp, nothing.
  return false;
}

void keyboard_report(byte port, Gamecube_Report_t &gc_report) {
  for (byte i=0; i<3; i++) {
    // Get the previous known state of the keystate index.
    byte key = gc_keyboard_last_press[port][i];

    // Check if our USB keyboard is reporting this key as pressed right now.
    bool prevState = gc_keyboard_pressed[key];

    // Check if the current state of the GC keyboard is reporting we are pressing this key.
    bool curState = is_pressed(gc_report, key);

    // If the states are different, we either pressed or released it.
    if (prevState != curState) {
      
      // Only update the state if it's a known key
      if (key >= GCKEY_HOME) keyboard_onState(key, curState);

      // Mark this key as either pressed or released for our USB state.
      gc_keyboard_pressed[key] = curState;
    }

    // Update our previous known state array for the next report read.
    gc_keyboard_last_press[port][i] = gc_report.keyboard.keypress[i];
  }
}

void keyboard_onState(byte key, bool state) {
  // Try our best to convert to a standard US keyboard.
  switch (key) {
    case GCKEY_HOME:
      keyboard_press(KEY_HOME, state);
      break;
    case GCKEY_END:
      keyboard_press(KEY_END, state);
      break;
    case GCKEY_PAGEUP:
      keyboard_press(KEY_PAGE_UP, state);
      break;
    case GCKEY_PAGEDOWN:
      keyboard_press(KEY_PAGE_DOWN, state);
      break;
    case GCKEY_SCROLLLOCK:
      keyboard_press(KEY_SCROLL_LOCK, state);
      break;
      
    case GCKEY_A:
      keyboard_press('a', state);
      break;
    case GCKEY_B:
      keyboard_press('b', state);
      break;
    case GCKEY_C:
      keyboard_press('c', state);
      break;
    case GCKEY_D:
      keyboard_press('d', state);
      break;
    case GCKEY_E:
      keyboard_press('e', state);
      break;
    case GCKEY_F:
      keyboard_press('f', state);
      break;
    case GCKEY_G:
      keyboard_press('g', state);
      break;
    case GCKEY_H:
      keyboard_press('h', state);
      break;
    case GCKEY_I:
      keyboard_press('i', state);
      break;
    case GCKEY_J:
      keyboard_press('j', state);
      break;
    case GCKEY_K:
      keyboard_press('k', state);
      break;
    case GCKEY_L:
      keyboard_press('l', state);
      break;
    case GCKEY_M:
      keyboard_press('m', state);
      break;
    case GCKEY_N:
      keyboard_press('n', state);
      break;
    case GCKEY_O:
      keyboard_press('o', state);
      break;
    case GCKEY_P:
      keyboard_press('p', state);
      break;
    case GCKEY_Q:
      keyboard_press('q', state);
      break;
    case GCKEY_R:
      keyboard_press('r', state);
      break;
    case GCKEY_S:
      keyboard_press('s', state);
      break;
    case GCKEY_T:
      keyboard_press('t', state);
      break;
    case GCKEY_U:
      keyboard_press('u', state);
      break;
    case GCKEY_V:
      keyboard_press('v', state);
      break;
    case GCKEY_W:
      keyboard_press('w', state);
      break;
    case GCKEY_X:
      keyboard_press('x', state);
      break;
    case GCKEY_Y:
      keyboard_press('y', state);
      break;
    case GCKEY_Z:
      keyboard_press('z', state);
      break;
      
    case GCKEY_1:
      keyboard_press('1', state);
      break;
    case GCKEY_2:
      keyboard_press('2', state);
      break;
    case GCKEY_3:
      keyboard_press('3', state);
      break;
    case GCKEY_4:
      keyboard_press('4', state);
      break;
    case GCKEY_5:
      keyboard_press('5', state);
      break;
    case GCKEY_6:
      keyboard_press('6', state);
      break;
    case GCKEY_7:
      keyboard_press('7', state);
      break;
    case GCKEY_8:
      keyboard_press('8', state);
      break;
    case GCKEY_9:
      keyboard_press('9', state);
      break;
    case GCKEY_0:
      keyboard_press('0', state);
      break;
      
    case GCKEY_MINUS:
      keyboard_press(KEY_KP_MINUS, state);
      break;
    case GCKEY_LEFTBRACKET:
      keyboard_press('[', state);
      break;
    case GCKEY_RIGHTBRACKET:
      keyboard_press(']', state);
      break;
      
    case GCKEY_COMMA:
      keyboard_press(',', state);
      break;
    case GCKEY_PERIOD:
      keyboard_press('.', state);
      break;
    case GCKEY_SLASH:
      keyboard_press('/', state);
      break;
      
    case GCKEY_F1:
      keyboard_press(KEY_F1, state);
      break;
    case GCKEY_F2:
      keyboard_press(KEY_F2, state);
      break;
    case GCKEY_F3:
      keyboard_press(KEY_F3, state);
      break;
    case GCKEY_F4:
      keyboard_press(KEY_F4, state);
      break;
    case GCKEY_F5:
      keyboard_press(KEY_F5, state);
      break;
    case GCKEY_F6:
      keyboard_press(KEY_F6, state);
      break;
    case GCKEY_F7:
      keyboard_press(KEY_F7, state);
      break;
    case GCKEY_F8:
      keyboard_press(KEY_F8, state);
      break;
    case GCKEY_F9:
      keyboard_press(KEY_F9, state);
      break;
    case GCKEY_F10:
      keyboard_press(KEY_F10, state);
      break;
    case GCKEY_F11:
      keyboard_press(KEY_F11, state);
      break;
    case GCKEY_F12:
      keyboard_press(KEY_F12, state);
      break;
    case GCKEY_ESC:
      keyboard_press(KEY_ESC, state);
      break;
    case GCKEY_INSERT:
      keyboard_press(KEY_INSERT, state);
      break;
    case GCKEY_DELETE:
      keyboard_press(KEY_DELETE, state);
      break;
      
    case GCKEY_BACKSPACE:
      keyboard_press(KEY_BACKSPACE, state);
      break;
    case GCKEY_TAB:
      keyboard_press(KEY_TAB, state);
      break;
    case GCKEY_CAPSLOCK:
      keyboard_press(KEY_CAPS_LOCK, state);
      break;
    case GCKEY_LEFTSHIFT:
      keyboard_press(KEY_LEFT_SHIFT, state);
      break;
    case GCKEY_RIGHTSHIFT:
      keyboard_press(KEY_RIGHT_SHIFT, state);
      break;
    case GCKEY_LEFTCTRL:
      keyboard_press(KEY_LEFT_CTRL, state);
      break;
    case GCKEY_LEFTALT:
      keyboard_press(KEY_LEFT_ALT, state);
      break;

    case GCKEY_SPACE:
      keyboard_press(' ', state);
      break;

    case GCKEY_LEFT:
      keyboard_press(KEY_LEFT_ARROW, state);
      break;
    case GCKEY_DOWN:
      keyboard_press(KEY_DOWN_ARROW, state);
      break;
    case GCKEY_UP:
      keyboard_press(KEY_UP_ARROW, state);
      break;
    case GCKEY_RIGHT:
      keyboard_press(KEY_RIGHT_ARROW, state);
      break;
      
    case GCKEY_ENTER:
      keyboard_press(KEY_RETURN, state);
      break;
      
    default:
      break;
  }
}

void keyboard_press(uint8_t key, bool state) {
  if (state) {
    Keyboard.press(key);
  } else {
    Keyboard.release(key);
  }
}
