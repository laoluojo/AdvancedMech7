//#include <AFMotor.h>
#include <NewPing.h>
#include <Servo.h>
#include <avr/io.h>
#include <avr/interrupt.h>
//Hi there
/*
   NOTE:
        xxx_L represents a variable specifically for the LEFT motor of the robot (Looking forward with it)
        xxx_R represents a variable specifically for the RIGHT motor of the robot (Looking forward with it)

    REV 3:
          - Implemented wheel speed PID
          - Specified wheel variables as linear
          - Created Angular wheel variables
    REV 2:
          - Introduced a "power factor to the movement functions (e.g forward(x)"
          - Added ultrasonic program, including it's PWM
          - Changed "dia_L" to 39.
*/

/**********************************************************
   --------------------- Definitions ----------------------
 **********************************************************/

// -------------------- PIN Definitions --------------------

// Pins to set rotation direction
#define IN1_PIN_L 2
#define IN2_PIN_L 3
#define IN3_PIN_R 5 //4
#define IN4_PIN_R 6 //5

// Enable Pins: For "Enabling" the motors to rotate
#define EN_PIN_L 4 //6
#define EN_PIN_R 7

// Pins to recevie Encoder signals
#define ENCOD1_PIN_L 25
#define ENCOD2_PIN_L 26
#define ENCOD3_PIN_R 27
#define ENCOD4_PIN_R 28

//Ultrasonic Sensor Definitions
#define TRIGGER_PIN1  8  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN1     9  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 20.0 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
#define MIN_DISTANCE 2.0 // Minimum distance

#define TRIGGER_PIN2  10  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN2     11  // Arduino pin tied 

//IR Sensor Definitions
#define IR_Servo_Reading 31    //IR Servo reading
#define IR_Front_Reading 33    //IR Front reading

// -------------------- Variable Definitions --------------------

// #################### Encoder Keeping ####################
volatile long ECount_L = 0;
volatile long ECount_R = 0;
long prev_ECount_L = 0;
long prev_ECount_R = 0;

// #################### Vehicle Parameters ####################
const float dia_L = 32.0;
const float dia_R = 32.0;
const float wheelBase = 87.5;
const int clicksPerRev = 2940;
const float cal_Factor_L = 0.0;
const float cal_Factor_R = 0.0;

// #################### Miner Location ####################
struct Miner {
  double xPos;
  double yPos;
};

// #################### Vehicle Information  ####################
struct Vehicle {
  double prev_xPos = 0.0;
  double prev_yPos = 0.0;
  double curr_xPos = 0.0;
  double curr_yPos = 0.0;
  double initial_xPos = 0.0;
  double initial_yPos = 0.0;
  double curr_Pos;
  double prev_Orien;
  double curr_Orien;
  double initial_Orien = 0.0;
  double curr_LinVel;
  double curr_Acc;
  double turn_Radius;
  double del_Ang;
  double del_Dist;
};
Vehicle robo; // Declares and instance of vehicle

// #################### Wheel Information  ####################
struct Wheel {
  volatile long ECount = 0;
  long prev_ECount = 0;

  double prev_LinDist = 0.0;
  double prev_LinVel = 0.0;
  double curr_LinDist;
  double curr_LinVel;
  double del_Dist = 0.0;

  double curr_AngVel;


};
Wheel whl_L;
Wheel whl_R;


// #################### Time Keeping ####################
double start_Time;
struct Timer {
  double curr_Time;
  double prev_Time;
  double dt() {
    return curr_Time - prev_Time;
  }

  double timeInSecs()
  {
    return ((millis() / 1000.0) - start_Time);
  }
};

Timer tim_L;
Timer tim_R;

// #################### Ultrasonic Sensor Variables ####################
NewPing sonar(TRIGGER_PIN1, ECHO_PIN1, MAX_DISTANCE + 20); // NewPing setup of pins and maximum distance.
NewPing sonar2(TRIGGER_PIN2, ECHO_PIN2, MAX_DISTANCE + 20); // NewPing setup of pins and maximum distance.
int ultra_Val_L = 0;
int ultra_Val_R = 0;

// #################### IR Sensor Variables ####################
Servo IR_Servo;
int pos = 0;                            // variable to store the servo position
float Distance_IRServo;
float Distance_IRFront;

// #################### PID ####################
struct PID {
  double Ki;
  double Kp;
  double Kd;
  double integral = 0.0;
  double derivative = 0.0;
  double lastError = 0.0;
  double error = 0.0;
  double pid;
};

PID U_L_PID; // PID for the left Ultrasonic
PID U_R_PID;
PID E_L_PID;
PID E_R_PID;

// #################### MISC ####################
const double pi = 3.14159265359;
double waitTimer[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
int targetPWM = 70; // Baseline pwm for main motors
double targetSpeed = 1.7; // Baseline rotational speed for main motors (in rad/sec)
const double wall_Length = 17.8; // distance from pillar to pillar in cms
int counter = 1;
bool completed = false;
int targetDist = 5;
double prev_Dist = 0.0 ;
double prev_Ang = 0.0;

double prevvy = 0.0;

// ************************* End of Definitions *********************************



void setup()
{
  Serial.begin(115200);
  // set all the motor control pins to outputs
  pinMode(EN_PIN_L, OUTPUT);
  pinMode(EN_PIN_R, OUTPUT);
  pinMode(IN1_PIN_L, OUTPUT);
  pinMode(IN2_PIN_L, OUTPUT);
  pinMode(IN3_PIN_R, OUTPUT);
  pinMode(IN4_PIN_R, OUTPUT);
  pinMode(ENCOD1_PIN_L, INPUT);
  pinMode(ENCOD2_PIN_L, INPUT);
  pinMode(ENCOD3_PIN_R, INPUT);
  pinMode(ENCOD4_PIN_R, INPUT);

  // IR servo control set up
  IR_Servo.attach(29);                                            // servo to Digital pin 9
  IR_Servo.write(0);                                            // set servo to 0
  pinMode(IR_Servo_Reading, INPUT);                                     //declare pin 31 as input
  pinMode(IR_Front_Reading, INPUT);                                     //declare pin 33 as input

  // Setup Interrupts
  attachInterrupt(digitalPinToInterrupt(ENCOD1_PIN_L), Encod_ISR_L, RISING); // interrrupt 1 is data ready
  attachInterrupt(digitalPinToInterrupt(ENCOD3_PIN_R), Encod_ISR_R, RISING); // interrrupt 1 is data ready RISING

  E_L_PID.Kp = 0.35; //0.8;
  E_L_PID.Ki = 0.1; //0.2
  E_L_PID.Kd = 0.3; //0.3; //0.7;

  E_R_PID.Kp = 0.4; //0.4;
  E_R_PID.Ki = 0.1; //0.1;
  E_R_PID.Kd = 0.3; //0.3;

  U_L_PID.Kp = 2.0; //5.0;
  U_L_PID.Ki = 0.0; //0.3;
  U_L_PID.Kd = 2.0;

  U_R_PID.Kp = 2.0; //5.0;
  U_R_PID.Ki = 0.0; //0.3;
  U_R_PID.Kd = 2.0;

  robo.prev_Orien = robo.initial_Orien;
  robo.prev_xPos = robo.initial_xPos;
  robo.prev_yPos = robo.initial_yPos;

  delay(5000);
  //counter = 14;

  start_Time = millis() / 1000;
}

int i = 0;

void loop()
{

// DEBUG CODE
  if ((millis() - waitTimer[0]) > 200)
  {
    //IR_Sensor();

    //Forward(U_PID.pid);
    //Leftward(E_L_PID.pid, E_R_PID.pid);
    Forward(E_L_PID.pid, 0); //, E_R_PID.pid);
    



    //Serial.print ("time in secs : "); //whl_L.curr_AngVel : ");
    // Serial.print (millis()/1000.0); //whl_L.curr_AngVel); //robo.curr_LinVel);
    // Serial.print ("\t");
    //    Serial.print ("targetSpeed : "); //whl_L.curr_AngVel : ");
    //    Serial.print (targetSpeed); //whl_L.curr_AngVel); //robo.curr_LinVel);
    //    Serial.print ("\t");
    //    Serial.print ("whl_L.curr_AngVel : "); //whl_L.curr_AngVel : ");
    //    Serial.print (whl_L.curr_AngVel);
    //    Serial.print ("\t");
    //    Serial.print ("whl_R.curr_AngVel : "); //whl_L.curr_AngVel : ");
    //    Serial.println (whl_R.curr_AngVel);


    waitTimer[0] = millis();
  }

  //  if (robo.curr_xPos > 200 - 0.7)
  //  {
  //    robo_Halt();
  //    Serial.print("robo.curr_Orien : ");
  //    Serial.println(rad2Deg(robo.curr_Orien));
  //    while (1) {}
  //  }

  //   if (rad2Deg(robo.curr_Orien) > 80) //zrobo.curr_xPos > 400)//  //curr_Pos
  //   {
  //      waitInSecs(0.01);
  //      Forward(E_L_PID.pid, E_R_PID.pid);
  //      Serial.print("robo.curr_xPos : ");
  //      Serial.print(robo.curr_xPos);
  //      Serial.print("\t");
  //      if (robo.curr_xPos > 50)
  //     {
  //       robo_Halt();
  //      Serial.print("robo.curr_Orien : ");
  //      Serial.println(rad2Deg(robo.curr_Orien));
  //      while(1){}
  //     }
  //
  //   }

//        Serial.print("Counter: ");
//        Serial.println(counter);
        
/*  switch (counter)
  {
    // 
//case 0 :
////        Serial.print("Rigt: ");
////        Serial.print(ultra_Val_R);
////        Serial.print("\t");
////        Serial.print("R - Error: ");
////        Serial.print(U_R_PID.error);
////        Serial.print("\t");
////        Serial.print("R - PID Val: ");
////        Serial.println(U_R_PID.pid);
//Serial.print("Speed left: ");
//
//        Serial.print(whl_L.curr_AngVel);
//        Serial.print("\t");
//        Serial.print("Speed Right ");
//        Serial.println(whl_R.curr_AngVel);
//Forward(-U_L_PID.pid);
//break;
    
// Stop and proceed to the next when the robot sees an opening to it's left
    case 1 :
      if (ultra_Val_L < 20  )
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);

//        //Serial.print("\t");
//        Serial.print("Left: ");
//        Serial.println(ultra_Val_L);
        //        Serial.print("Right: ");
        //        Serial.println(ultra_Val_R);
      }
      else {
        robo_Halt();

        counter++;
        //delay(100);
        prev_Dist = robo.curr_xPos;
//        Serial.print("prev_Dist: ");
//        Serial.println(prev_Dist);
      }
      break;

    // Stop and proceed to the next when the robot moves forward 75 mm
    // to center itself for turning at 1
    case 2 :
      Forward(E_L_PID.pid, E_R_PID.pid);
      Serial.print("robo.curr_xPos - prev_Dist: ");
        Serial.println(robo.curr_xPos - prev_Dist);
      if ((robo.curr_xPos - prev_Dist) > 75)
      {
        robo_Halt();
        counter++;
        //delay(100);
        prev_Ang = robo.curr_Orien;
      }
      break;
    // Stop and proceed to the next when the robot rotates approximately 90 degs
    // to the left at 1
    case 3 :
      Leftward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) < -90 + 7)
      {
        robo_Halt();
        counter++;
      }
      break;

    // Stop and proceed to the next when the robot's  front IR sensor is 8 cm away at 1
    case 4 :
//      Serial.print("Distance_IRFront: ");
//      Serial.println(Distance_IRFront);
      if (Distance_IRFront > 8)
      {
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
      }
      break;

    // Stop and proceed to the next when the robot rotates approximately 90 degs
    // to the right at 2
    case 5 :
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
      }
      break;

    // Stop and proceed to the next when the robot's  front IR sensor is 8 cm away at 2
    case 6 :
      if (Distance_IRFront > 8)
      {
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
      }
      break;
    // Stop and proceed to the next when the robot rotates approximately 90 degs
    // to the left at 2
    case 7:

      Leftward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) < -90 + 13)
      {
        robo_Halt();
        counter++;
      }
      break;

// Stop and proceed to the next when the robot's  front IR sensor is 8 cm on a long strecth from 2 - 3
    case 8 :
      if (Distance_IRFront > 8)
      {
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the left at 3
    case 9 :
      Leftward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) < -90 + 7)
      {
        robo_Halt();
        counter++;
      }
      break;
      
// Stop and proceed to the next when the robot's  front IR sensor is 8 cm on a long strecth from 3 - 4
    case 10 :
    Serial.println(Distance_IRFront);
      if (Distance_IRFront > 8)
      {
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the right at 4
    case 11 :
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
      }
// ** Working so far till the turm at 4
break;

// Stop and proceed to the next when the robot's  front IR sensor is 8 cm on a short strecth at 4
    case 12 :
    Serial.println(Distance_IRFront);
      if (Distance_IRFront > 8)
      {
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        //prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the right between 4 and 5
    case 13 :
    Serial.println("Hello");
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
        //prev_Ang = robo.curr_Orien;
        //prev_Dist = robo.curr_xPos;
      }
      break;

//   case 14 :
//    Serial.println(Distance_IRFront);
//      if (Distance_IRFront > 8)
//      {
//        Forward(E_L_PID.pid, E_R_PID.pid);
//      }
//      else {
//        robo_Halt();
//        counter++;
//        //prev_Ang = robo.curr_Orien;
//        prev_Dist = robo.curr_xPos;
//      }
//      break;
//      
case 14 :
      if (ultra_Val_L < 20  )
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);

//        //Serial.print("\t");
//        Serial.print("Left: ");
//        Serial.println(ultra_Val_L);
        //        Serial.print("Right: ");
        //        Serial.println(ultra_Val_R);
      }
      else {
        robo_Halt();

        counter++;
        //delay(100);
        prev_Dist = robo.curr_xPos;
        Serial.print("prev_Dist: ");
        Serial.println(prev_Dist);
      }
      break;

    // Stop and proceed to the next when the robot moves forward 75 mm
    // to center itself for turning at 1
    case 15 :
      Forward(E_L_PID.pid, E_R_PID.pid);
      Serial.print("robo.curr_xPos : ");
        Serial.println(robo.curr_xPos);
      if ((prev_Dist - robo.curr_xPos) > 75)
      {
        robo_Halt();
        counter++;
        //delay(100);
        prev_Ang = robo.curr_Orien;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the left at 5
    case 16 :
      Leftward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) < -90 + 7)
      {
        robo_Halt();
        counter++;
        //prev_Ang = robo.curr_Orien;
        //prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot's  front IR sensor is 8 cm away at 5
    case 17 :
      if (Distance_IRFront > 8)
      {
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        //prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the right between 5 and 6 (Turning to the ramp)
    case 18 :
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot sees an opening to it's Right while moving from 5 - 6
    case 19 :
      if (ultra_Val_R < 20  )
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot's front IR sensor is 8 cm away at 6
    case 20 :
      if (Distance_IRFront > 8)
      {
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the right at 6 (Turning on the ramp)
    case 21 :
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot's front IR sensor is 8 cm away at 7 (Going from 6-7)
    case 22 :
      if (Distance_IRFront > 8)
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the right at 7 (Turning on the ramp)
    case 23 :
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot sees an opening to it's Right while moving from 7 - 8
    case 24 :
      if (ultra_Val_R < 20  )
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot moves forward 75 mm
// to center itself for turning at 8
    case 25 :
      Forward(E_L_PID.pid, E_R_PID.pid);
      if ((robo.curr_xPos - prev_Dist) > 75)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the right at 8
    case 26 :
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot's front IR sensor is 8 cm away at 8 
    case 27 :
      if (Distance_IRFront > 8)
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the left before 9
    case 28 :
      Leftward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) < -90 + 7)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot's front IR sensor is 8 cm away at (9) 
    case 29 :
      if (Distance_IRFront > 8)
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot moves forward 255 mm
// to make it from 9 to 10
    case 30 :
      //Forward(U_L_PID.pid);
      Forward(E_L_PID.pid, E_R_PID.pid);
      if ((robo.curr_xPos - prev_Dist) > 300)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;
      
// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the right at 10
    case 31 :
      Rightward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) > 90 - 10)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot's front IR sensor is 8 cm away at (11) 
    case 32 :
      if (Distance_IRFront > 8)
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;      

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the left at 11
    case 33 :
      Leftward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) < -90 + 7)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot moves forward 178 mm
// to make it from 11 to 12
    case 34 :
      //Forward(U_R_PID.pid);
      Forward(E_L_PID.pid, E_R_PID.pid);
      if ((robo.curr_xPos - prev_Dist) > 170)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot rotates approximately 90 degs
// to the left at 12
    case 35 :
      Leftward(E_L_PID.pid, E_R_PID.pid);
      if (rad2Deg(robo.curr_Orien - prev_Ang) < -90 + 7)
      {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;

// Stop and proceed to the next when the robot's front IR sensor is 8 cm away at (13) 
    case 36 :
      if (Distance_IRFront > 8)
      {
        //Forward(U_R_PID.pid);
        Forward(E_L_PID.pid, E_R_PID.pid);
      }
      else {
        robo_Halt();
        counter++;
        prev_Ang = robo.curr_Orien;
        prev_Dist = robo.curr_xPos;
      }
      break;  
// Notify completion
    case 37 :
      IR_Servo_Scan();
      break;        
      
    default:
      counter = counter;
  }*/

  // Updates all variables for calculations every 100 ms
  if ((millis() - waitTimer[1]) > 50)
  {
    Serial.print (targetSpeed); //whl_L.curr_AngVel); //robo.curr_LinVel);
    Serial.print ("\t");
    Serial.print (whl_L.curr_AngVel);
    Serial.print ("\t");
    Serial.println(E_L_PID.pid);
    cli();
    long EL = ECount_L;
    long ER = ECount_R;
    sei();

    int delta_L = EL - prev_ECount_L;
    int delta_R = ER - prev_ECount_R;

    prev_ECount_L = EL;
    prev_ECount_R = ER;

    // Get the current speed of the motors
    whl_L.curr_AngVel = whl_AngVel_L(delta_L);
    whl_R.curr_AngVel = whl_AngVel_R(delta_R);

    // Update change in wheel distances
    whl_L.del_Dist = whlDeltaD_L(delta_L);
    whl_R.del_Dist = whlDeltaD_R(delta_R);

    // Calculate the PID
    U_L_PID.error = ultra_L_Error();
    U_L_PID.integral = U_L_PID.integral + U_L_PID.error;
    U_L_PID.derivative = U_L_PID.error - U_L_PID.lastError;
    U_L_PID.pid = (U_L_PID.Kp * U_L_PID.error) + (U_L_PID.Ki * U_L_PID.integral) + (U_L_PID.Kd * U_L_PID.derivative);
    U_L_PID.lastError = U_L_PID.error;

    U_R_PID.error = ultra_R_Error();
    U_R_PID.integral = U_R_PID.integral + U_R_PID.error;
    U_R_PID.derivative = U_R_PID.error - U_R_PID.lastError;
    U_R_PID.pid = (U_R_PID.Kp * U_R_PID.error) + (U_R_PID.Ki * U_R_PID.integral) + (U_R_PID.Kd * U_R_PID.derivative);
    U_R_PID.lastError = U_R_PID.error;

    E_L_PID.error = encodError(targetSpeed, whl_L);
    E_L_PID.integral = E_L_PID.integral + E_L_PID.error;
    E_L_PID.derivative = E_L_PID.error - E_L_PID.lastError;
    E_L_PID.pid = (E_L_PID.Kp * E_L_PID.error) + (E_L_PID.Ki * E_L_PID.integral) + (E_L_PID.Kd * E_L_PID.derivative);
    E_L_PID.lastError = E_L_PID.error;

    E_R_PID.error = encodError(targetSpeed, whl_R);
    E_R_PID.integral = E_R_PID.integral + E_R_PID.error;
    E_R_PID.derivative = E_R_PID.error - E_R_PID.lastError;
    E_R_PID.pid = (E_R_PID.Kp * E_R_PID.error) + (E_R_PID.Ki * E_R_PID.integral) + (E_R_PID.Kd * E_R_PID.derivative);
    E_R_PID.lastError = E_R_PID.error;



    //  Serial.print ("whl_L.curr_AngVel : ");
    //  Serial.print (whl_L.curr_AngVel);
    // Serial.print ("\t");
    //Serial.print ("U_PID.error : ");
    // Serial.println (U_PID.error);

    //    robo.curr_Pos = roboDist();

    // Update Distances travelled by both wheels in mm
    //    whl_L.curr_LinDist = whl_Dist_L();
    //    whl_R.curr_LinDist = whl_Dist_R();

    // Update linear Wheel Velocities in mm/sec
    //    whl_L.curr_LinVel = whl_LinVel_L(whl_L, tim_L);
    //    whl_R.curr_LinVel = whl_LinVel(whl_R, tim_R);

    // Update Delta Distance
    robo.del_Dist = roboDel_Dist();

    // Update Location of Robot
    robo.curr_yPos = roboDist_Y();
    robo.curr_xPos = roboDist_X();


    // Update Delta Angle
    robo.del_Ang = roboDel_Ang();

    // Update Robot Orientation
    robo.curr_Orien = roboAngle();

    Distance_IRFront = IR_Sensor(IR_Front_Reading);
    Distance_IRServo = IR_Sensor(IR_Servo_Reading);
    
    // Update Robot Velocity
    //    robo.curr_LinVel = roboVel();

    // Update Robot Acceleration
    //    robo.curr_Acc = roboAcc();

    // Update Turn Radius
    //    robo.turn_Radius = turn_Radius();

    prevvy = robo.curr_Orien;
    //    Serial.print("robo.prev_Orien : "); //whl_L.curr_AngVel : ");
    //    Serial.println (rad2Deg(prevvy));
    robo.prev_xPos = robo.curr_xPos;
    robo.prev_yPos = robo.curr_yPos;


    waitTimer[1] = millis();
  }
}

/**********************************************************
  --------------------- FUNCTIONS ----------------------
 **********************************************************/

// -------------------- ISRs --------------------
void Encod_ISR_L()
{
  if (digitalRead(ENCOD2_PIN_L) == LOW)
  {
    /** and counts down when move from R to L **/
    ECount_L--;
  }
  else
  {
    /** Encoder counts up when move from L to R **/
    ECount_L++;
  }
  //   Serial.print("ECount_L : ");
  //   Serial.println(ECount_L);
}

void Encod_ISR_R()
{
  if (digitalRead(ENCOD4_PIN_R) == LOW)
  {
    /** and counts down when move from R to L **/
    ECount_R--;
  }
  else
  {
    /** Encoder counts up when move from L to R **/
    ECount_R++;
  }
  //   Serial.print("ECount_R : ");
  //   Serial.println(ECount_R);
}

// -------------------- Motion Control Functions --------------------

// Robot Forward function for the Ultrasonics
void Forward(double pidResult)
{
  digitalWrite(IN1_PIN_L, LOW);
  digitalWrite(IN2_PIN_L, HIGH);
  analogWrite(EN_PIN_L, (RpS2pwm_L(targetSpeed) + pidResult - 9)); //-13

  digitalWrite(IN3_PIN_R, LOW);
  digitalWrite(IN4_PIN_R, HIGH);
  analogWrite(EN_PIN_R, (RpS2pwm_R(targetSpeed) - pidResult));
}

// Robot Forward function for the Encoders
void Forward(double E_pid_L, double E_pid_R)
{
  digitalWrite(IN1_PIN_L, LOW);
  digitalWrite(IN2_PIN_L, HIGH);
  analogWrite(EN_PIN_L, (RpS2pwm_L(targetSpeed - E_pid_L)));

  digitalWrite(IN3_PIN_R, LOW);
  digitalWrite(IN4_PIN_R, HIGH);
  analogWrite(EN_PIN_R, (RpS2pwm_R(targetSpeed - E_pid_R)));
}

void Backward(int spd_L, int spd_R)
{
  digitalWrite(IN1_PIN_L, HIGH);
  digitalWrite(IN2_PIN_L, LOW);
  analogWrite(EN_PIN_L, spd_L - 14);

  digitalWrite(IN3_PIN_R, HIGH);
  digitalWrite(IN4_PIN_R, LOW);
  analogWrite(EN_PIN_R, spd_R);
}

void Leftward(double E_pid_L, double E_pid_R)
{
  digitalWrite(IN1_PIN_L, HIGH);
  digitalWrite(IN2_PIN_L, LOW);
  analogWrite(EN_PIN_L, RpS2pwm_L(targetSpeed - E_pid_L)); // - 14);

  digitalWrite(IN3_PIN_R, LOW);
  digitalWrite(IN4_PIN_R, HIGH);
  analogWrite(EN_PIN_R, RpS2pwm_R(targetSpeed - E_pid_R));
}

void Rightward(double E_pid_L, double E_pid_R)
{
  digitalWrite(IN1_PIN_L, LOW);
  digitalWrite(IN2_PIN_L, HIGH);
  analogWrite(EN_PIN_L, RpS2pwm_L(targetSpeed - E_pid_L)); // - 14);

  digitalWrite(IN3_PIN_R, HIGH);
  digitalWrite(IN4_PIN_R, LOW);
  analogWrite(EN_PIN_R, RpS2pwm_R(targetSpeed - E_pid_R));
}

void robo_Halt()
{
  digitalWrite(IN1_PIN_L, LOW);
  digitalWrite(IN2_PIN_L, LOW);
  digitalWrite(IN3_PIN_R, LOW);
  digitalWrite(IN4_PIN_R, LOW);
}

//-------------------------IR Sensors-------------------------
double IR_Sensor(int pin) {
  double IR_Value = analogRead(pin); //take reading from sensor

  float Raw_Voltage_IR = IR_Value * 0.00322265625; //convert analog reading to voltage (5V/1024bit=0.0048828125)(3.3V/1024bit =0.00322265625)
  float Distance_IR = -30.195 * Raw_Voltage_IR + 71.35;
  return Distance_IR;
}
double IR_Servo_Scan() {

  for (pos = 0; pos <= 40; pos += 1) {          // goes from 0 degrees to 60 degrees
    IR_Servo.write(pos);                           // tell servo to go to position in variable 'pos'
    if (pos == 0 || pos == 10 || pos == 20 || pos == 30 || pos == 40) {   
      Distance_IRServo = IR_Sensor(IR_Servo_Reading);
      Serial.print("Servo IR Distance: ");
      Serial.println(Distance_IRServo);
      return Distance_IRServo;
    }
    delay(15);                       // waits 15ms for the servo to reach the position
  }
 
  for (pos = 40; pos >= 0; pos -= 1) {                      // goes from 60 degrees to 0 degrees
    IR_Servo.write(pos);                                     // tell servo to go to position in variable 'pos'
    if (pos == 0 || pos == 10 || pos == 20 || pos == 30 || pos == 40) {      
      Distance_IRServo = IR_Sensor(IR_Servo_Reading);
      Serial.print("Servo IR Distance: ");
      Serial.println(Distance_IRServo);
      return Distance_IRServo;
    }
    delay(15);                       // waits 15ms for the servo to reach the position
  }
} 

void IRtest() {
  double IR_Servo_Value;
  double IR_Front_Value;
  IR_Servo.write(20);                           // tell servo to go to position in variable 'pos'

  IR_Servo_Value = analogRead(IR_Servo_Reading); //take reading from sensor
  IR_Front_Value = analogRead(IR_Front_Reading); //take reading from sensor
  
  float Raw_Voltage_IRServo = IR_Servo_Value * 0.00322265625; //convert analog reading to voltage (5V/1024bit=0.0048828125)(3.3V/1024bit =0.00322265625)
  float Distance_IRServo = -30.195 * Raw_Voltage_IRServo + 71.35;
  float Raw_Voltage_IRFront = IR_Front_Value * 0.00322265625; //convert analog reading to voltage (5V/1024bit=0.0048828125)(3.3V/1024bit =0.00322265625)
  float Distance_IRFront = -30.195 * Raw_Voltage_IRFront + 71.35;
  Serial.print("Servo IR Distance: ");
  Serial.println(Distance_IRServo);
  Serial.print("Front IR Distance: ");
  Serial.println(Distance_IRFront);

  delay(100); //create a delay of 0.1s
}

// -------------------- Robot Localization Functions --------------------




// -------------------- PID Functions --------------------

// Solves for the difference in the rotational speed of the wheel compared to a set value (in rad/sec)
double encodError(double setVal, Wheel W) {
  return abs(W.curr_AngVel) - setVal;
}

// Solves for the difference between the left ultrasonic sensor and 8cm from left wall
double ultra_L_Error() {
  int distanceGood_L = 0;

  ultra_Val_L = sonar.ping_cm();
  delay(10);
  if (ultra_Val_L < MAX_DISTANCE && ultra_Val_L > MIN_DISTANCE) {
    distanceGood_L = ultra_Val_L;
  }
  else if (ultra_Val_L > MAX_DISTANCE) {
    distanceGood_L = MAX_DISTANCE;
    ultra_Val_L = MAX_DISTANCE;
  }
  else {
    ultra_Val_L = MAX_DISTANCE;
    distanceGood_L = MAX_DISTANCE;
  }
//  Serial.print("distanceGood_L: ");
//  Serial.print(distanceGood_L);
//  Serial.print("\t");
  return distanceGood_L - targetDist;
}


// Solves for the difference between the right ultrasonic sensor and 8cm from right wall
double ultra_R_Error() {

  int distanceGood_R = 0;
  delay(10);
  ultra_Val_R = sonar2.ping_cm();
  //  Serial.print("ultra_Val_R : ");
  //    Serial.println(ultra_Val_R);
  if (ultra_Val_R < MAX_DISTANCE && ultra_Val_R > MIN_DISTANCE) {
    distanceGood_R = ultra_Val_R;
  }
  else if (ultra_Val_R > MAX_DISTANCE) {
    distanceGood_R = MAX_DISTANCE;
    ultra_Val_R = MAX_DISTANCE;
  }
  else {
    ultra_Val_R = MAX_DISTANCE;
    distanceGood_R = MIN_DISTANCE;
  }
//Serial.print("distanceGood_R: ");
//  Serial.print(distanceGood_R);
//  Serial.print("\t");
  //delay(50);
  return distanceGood_R - targetDist;
}



// -------------------- Formulas --------------------

// #################### Wheel related Formulas ####################

// Left Wheel Linear Distance in mm
double whl_Dist_L()
{
  return distPerClick(dia_L) * ECount_L;
}

// Riight Wheel Linear Distance in mm
double whl_Dist_R()
{
  return distPerClick(dia_R) * ECount_R;
}

double whlDeltaD_L(int del)
{
  double ans = distPerClick(dia_L) * (del);

  return ans;
}

double whlDeltaD_R(int del)
{
  double ans = distPerClick(dia_L) * (del);

  return ans;
}


//returns wheel linear velocity in mm/sec
double whl_LinVel(Wheel w, Timer tim)
{
  tim.curr_Time = tim.timeInSecs();
  w.curr_LinVel = (w.curr_LinDist - w.prev_LinDist) / tim.dt();
  tim.prev_Time = tim.curr_Time;
  w.prev_LinDist = w.curr_LinDist;

  return w.curr_LinVel;
}

double whl_AngVel_L(int del)
{
  tim_L.curr_Time = tim_L.timeInSecs();

  double dt = tim_L.dt();
  double angVel = ((del / dt) / clicksPerRev ) * 2 * pi;

  tim_L.prev_Time = tim_L.curr_Time;

  return angVel;
}

double whl_AngVel_R(int del)
{
  tim_R.curr_Time = tim_R.timeInSecs();
  double dt = tim_R.dt();
  double angVel = ((del / dt) / clicksPerRev ) * 2 * pi;

  tim_R.prev_Time = tim_R.curr_Time;
  return angVel;
}


// #################### Robot Formulas ####################

double roboDist_Y()
{
  return (robo.del_Dist * sin((robo.del_Ang / 2) + prevvy)) + robo.prev_yPos; //robo.initial_yPos - (robo.turn_Radius * (cos(robo.curr_Orien) - cos(robo.initial_Orien)));
}

double roboDist_X()
{
  return (robo.del_Dist * cos((robo.del_Ang / 2) + prevvy)) + robo.prev_xPos; // robo.initial_xPos + (robo.turn_Radius * (sin(robo.curr_Orien) - sin(robo.initial_Orien)));
}

double roboDist()
{
  return (whl_L.curr_LinDist + whl_R.curr_LinDist) / 2;
}

double roboDel_Dist()
{
  return (whl_L.del_Dist + whl_R.del_Dist) / 2;
}

double roboVel()
{
  return (whl_R.curr_LinVel + whl_L.curr_LinVel) / 2.0 * sin(robo.curr_Orien);
}

double roboAcc()
{
  return ((whl_R.curr_LinVel + whl_L.curr_LinVel) * (whl_R.curr_LinVel - whl_L.curr_LinVel)) / (2 * wheelBase) * cos(robo.curr_Orien);
}

double distPerClick(double dia)
{
  return 2 * (pi * dia) / clicksPerRev;
}

double roboAngle()
{
  return robo.del_Ang + prevvy; //((whl_R.curr_LinVel - whl_L.curr_LinVel) * tim_L.curr_Time()) / wheelBase + robo.initial_Orien;
}

double roboDel_Ang()
{
  return (whl_L.del_Dist - whl_R.del_Dist) / wheelBase; //((whl_R.curr_LinVel - whl_L.curr_LinVel) * tim_L.curr_Time()) / wheelBase + robo.initial_Orien;
}

double turn_Radius()
{
  return (wheelBase * (whl_R.curr_LinVel + whl_L.curr_LinVel)) / (2 * (whl_R.curr_LinVel - whl_L.curr_LinVel));
}




// #################### General Math Formulas ####################

// Converts radian angle to degrees
double rad2Deg(double radi)
{
  return (180 / pi) * radi;
}

// Converts degrees to radian
double deg2Rad(double degr)
{
  return (pi / 180) * degr;
}

// Converts pwm Value to radian/sec speed for the left motor
double pwm2RpS_L(int val)
{
  return 1.4082 * log(val) - 3.7683;// Formula gotten from approximating with a log function in excel
}

// Converts pwm Value to radian/sec speed for the right motor
double pwm2RpS_R(int val)
{
  //return -6E-05 * (val * val) + 0.0292* val + 0.5616;
  return 1.3757 * log(val) - 3.6931;// Formula gotten from approximating with a log function in excel
}

//Converts radians per second to pwm for the left motor
int RpS2pwm_L(double rps)
{
  return exp((rps + 3.7683) / 1.4082);
}

//Converts radians per second to pwm for the right motor
int RpS2pwm_R(double rps)
{
  // int x1 = (-0.0292 + sqrt((0.0292 * 0.0292) - (4 * -6E-05 * (0.5616 - rps)))) / (2 * -6E-05);
  // int x2 = (-0.0292 - sqrt((0.0292 * 0.0292) - (4 * -6E-05 * (0.5616 - rps)))) / (2 * -6E-05);

  return exp((rps + 3.6931) / 1.3757);
}

void waitInSecs(double period)
{
  long t1 = millis();
  while (((millis() - t1) / 1000.0) < period) {}
}




/*
  // drives straight based on distance from left wall
    void leftFollow() {
      if((millis() - waitTimer[0]) > 100)
  {
    left_Error = ultra_L_Error();
    integral = integral + left_Error;
    derivative = left_Error - lastError;
    waitTimer[0] = millis();
    pidVal = (Kp_L*left_Error) + (Ki_L*integral) + (Kd_L*derivative);
    power_L = targetPower - pidVal;
    power_R = targetPower + pidVal;

     Serial.print("ultra_Val_L: ");
     Serial.print(distanceGood_L); // Send ping, get distance in cm and print result (0 = outside set distance range)
     Serial.print("cm");
     Serial.print("\t");
    Serial.print("error"); // Send ping, get distance in cm and print result (0 = outside set distance range)
    Serial.println(left_Error);
  }

  if((millis() - waitTimer[1]) > 200)
  {
    digitalWrite(IN1_PIN_L, LOW);
    digitalWrite(IN2_PIN_L, HIGH);
     //set speed to 200 out of possible range 0~255
     analogWrite(EN_PIN_L, power_L);
     //turn on motor B
    digitalWrite(IN3_PIN_R, LOW);
    digitalWrite(IN4_PIN_R, HIGH);
     //set speed to 200 out of possible range 0~255
    analogWrite(EN_PIN_R, power_R);
    lastError = left_Error;
    waitTimer[1] = millis();
  }

    }



  // drives straigt based on distance from right wall
    void rightFollow() {
      if((millis() - waitTimer[0]) > 100)
  {
    right_Error = ultra_R_Error();
    integral = integral + right_Error;
    derivative = right_Error - lastError;
    waitTimer[0] = millis();
    pidVal = (Kp_R*right_Error) + (Ki_R*integral) + (Kd_R*derivative);
    power_L = targetPower - pidVal;
    power_R = targetPower + pidVal;

     Serial.print("ultra_Val_L: ");
     Serial.print(distanceGood_R); // Send ping, get distance in cm and print result (0 = outside set distance range)
     Serial.print("cm");
     Serial.print("\t");
    Serial.print("error"); // Send ping, get distance in cm and print result (0 = outside set distance range)
    Serial.println(right_Error);
  }

  if((millis() - waitTimer[1]) > 200)
  {
    digitalWrite(IN1_PIN_L, LOW);
    digitalWrite(IN2_PIN_L, HIGH);
     //set speed to 200 out of possible range 0~255
     analogWrite(EN_PIN_L, power_L);
     //turn on motor B
    digitalWrite(IN3_PIN_R, LOW);
    digitalWrite(IN4_PIN_R, HIGH);
     //set speed to 200 out of possible range 0~255
    analogWrite(EN_PIN_R, power_R);
    lastError = right_Error;
    waitTimer[1] = millis();
  }

    }
*/







/**********************************************************
   ------------------- END of FUNCTIONS -------------------
 **********************************************************/
