# MicroBlaze_Stopwatch_7Segment_Controller

This project implements a MicroBlaze based stopwatch that displays time on an 8 digit seven segment display and is controlled using five push buttons. A hardware timer interrupt generates a precise 1 ms tick, enabling accurate real time counting with start, stop, direction control, and reset functionality.

## System Architecture

The stopwatch system is built around a MicroBlaze soft processor connected to an AXI Timer, AXI GPIO for button input, a seven segment display interface, and an interrupt controller. The timer generates periodic interrupts, while the processor handles timekeeping logic, button control, and display updates.

### RTL Block Design Inputs

<img src="./images/RTL_Block_Design_Inputs.png" width="800">

### RTL Block Design Outputs

<img src="./images/RTL_Block_Design_Outputs.png" width="800">

## Application Level Description

At the application level, the stopwatch maintains millisecond and second counters that are updated inside a timer interrupt service routine. The main loop continuously multiplexes the seven segment display and polls the button GPIO to detect edge triggered user input.

The stopwatch supports both count up and count down modes and prevents underflow when counting down.

### Button Controls

- **Right**: Start stopwatch and count up  
- **Left**: Stop stopwatch  
- **Up**: Set count up direction  
- **Down**: Set count down direction  
- **Center**: Reset time to 00:00.000  

## Timer Operation

The AXI Timer is configured in auto reload mode to generate an interrupt every 1 millisecond. This interrupt driven design allows accurate and deterministic timekeeping independent of display refresh timing.

### Timer Counting Behavior

<img src="./images/Timer_Counting.png" width="600">

### Timer Verification

The stopwatch timing was verified against an external reference to confirm correct operation and accuracy.

<img src="./images/Timer_matches_iPad.png" width="600">

## Summary

This project demonstrates interrupt driven timing, GPIO based user control, and seven segment display multiplexing on a MicroBlaze based FPGA system. It provides a practical example of real time embedded system design using FPGA based SoC architecture.

