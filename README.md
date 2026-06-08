# smart-medicine-box
IoT-based smart medicine box using ESP32, RTC, WiFi, and GSM for automated medication reminders, notifications, and emergency alerts

Designed to help users take medicines on time through multi-stage reminders and emergency alerts.

## Features

* Supports 2 users
* 3 medicine slots per user (6 total slots)
* Real-Time Clock (DS3231) scheduling
* LED and buzzer reminders
* Push notifications through Firebase Cloud Messaging (FCM)
* GSM/4G missed-call and SMS alerts
* IR sensor detection for medicine retrieval
* Manual acknowledgment button
* Multi-stage escalation system

## Hardware Components

* ESP32
* DS3231 RTC Module
* SIM7600 / SIM800 GSM Module
* IR Sensors (6)
* Push Buttons (6)
* LEDs (6)
* Buzzers (6)
* Power Supply

## System Workflow

### Stage 1 – Reminder

At the scheduled medicine time:

* LED turns ON
* Buzzer sounds slowly
* Mobile notification is sent

### Stage 2 – Escalation

If the medicine is not taken within 30 seconds:

* Faster LED flashing
* Faster buzzer
* GSM missed call
* SMS alert

### Missed Dose

If the medicine is still not taken after 90 seconds:

* Slot marked as MISSED
* Alert stops

### Acknowledgement

The reminder stops when:

* IR sensor detects compartment opening
  OR
* User presses acknowledgment button

## Software Architecture

State Machine:

IDLE → STAGE1 → STAGE2 → ACKED / MISSED

### States

* IDLE : Waiting for schedule
* STAGE1 : Initial reminder
* STAGE2 : Emergency escalation
* ACKED : Medicine taken
* MISSED : Dose missed

## Future Improvements

* Mobile application
* Cloud database logging
* Caregiver notifications
* OLED display
* Voice reminders
* Automatic pill dispensing
* Battery backup monitoring

## Author

Abhinay
B.Tech, IIT Bhubaneswar

## License

MIT License
