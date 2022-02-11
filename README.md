# ClimbingSystem

A simple system to allow the Third Person Character to attach and move on walls.

Press E to attach/detach from walls.
Once you are on a wall you can press the Jump button (SPACE) to climb faster, the character will jump and try to reattach to the wall when he reaches the apex.
When you reach the top of a wall, if there is enough space the character will also automatically vault up.
The system is based on boxcasts, that are shot in front of the character to determine the movement and most important the rotation of the character.

There are some parameters that can be tuned:
  MaxClimbAngle: when moving vertically, if the angle of the wall is higher than MaxClimbAngle, movement is stopped
  MinClimbAngle: when moving vertically, if the angle of the wall is lower than MinClimbAngle, movement is stopped
  MaxTurnAngle: when moving horizontally, if the angle of the wall is higher than MaxTurnAngle, movement is stopped
