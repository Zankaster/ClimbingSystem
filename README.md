# ClimbingSystem

A simple system to allow the Third Person Character to attach and move on walls.<br/>

Press E to attach/detach from walls.<br/>
Once you are on a wall you can press the Jump button (SPACE) to climb faster, the character will jump and try to reattach to the wall when he reaches the apex.<br/>
When you reach the top of a wall, if there is enough space the character will also automatically vault up.<br/>
The system is based on boxcasts, that are shot in front of the character to determine the movement and most important the rotation of the character.<br/>
It works best on regular surfaces, but also landscapes are supported.<br/>

There are some parameters that can be tuned:<br/>
  MaxClimbAngle: when moving vertically, if the angle of the wall is higher than MaxClimbAngle, movement is stopped<br/>
  MinClimbAngle: when moving vertically, if the angle of the wall is lower than MinClimbAngle, movement is stopped<br/>
  MaxTurnAngle: when moving horizontally, if the angle of the wall is higher than MaxTurnAngle, movement is stopped<br/>
    
https://user-images.githubusercontent.com/64004302/153592538-556535f5-50e4-4dee-864e-f32ac080202c.mp4

