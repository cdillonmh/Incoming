# Incoming
 Missile-Command-like game for Move38 Blinks by Dillon Hall
 
 Note: Requires [brunoga's custom blinklib](https://github.com/brunoga/blinklib) with datagrams disabled to install.

## Intro
A solo or asymmetric multiplayer game of protecting the Earth from asteroids spiraling towards it. Asteroids spawn from the edge of a hexagonal map, either in ever faster and more numerous waves in the solo mode or triggered by your opponent in multiplayer. Defend the earth by sending missiles to intercept the asteroids, taking into account their trajectory, missile speed/distance, and the time it takes to reload after each launch.

## Setup
Arrange 19 (or 37, untested but should work) blinks in a filled-hexagon pattern. You should see stars blinking in the background and the earth at the center. For solo play, click the earth and start defending!

For multiplayer, add one or more magenta asteroid launchers along the outer edge and click one of them instead to begin.

## Defending the Earth
As the defender in solo or multiplayer mode, your task is to use targeted missile strikes to eliminated asteroids as they spawn. Asteroids will pass from blink to blink as they head toward an eventual collision with Earth. They come in two types:
 - **Yellow**: Slow asteroids that can pass through up to 4 blinks on a "ring" before getting closer to the earth.
 - **Red**: Fast asteroids that will only pass through up to 2 blinks before getting closer to the earth.
 
 To destroy asteroids before they impact the earth, click a blink to target that space for a missile strike. The earth will send a missile to that space and, if you've timed it right (taking into account the path of the asteroid, the transit time of the missile, and the time it takes earth to reload), you'll have one less asteroid to worry about!
 
Each asteroid that impacts the earth causes 1 unit of damage. Accumulate 6 units of damage, and you lose.
 
You can target multiple spaces and the earth will fire missiles at each one when it's able. Note that the missiles are big but not terribly smart. They may get confused by too many targeting requests or nearby targets. Fewer, more accurate shots tends to be more effective than firing with reckless abandon.

## Attacking the Earth
As the magenta attacking player, your task is to time the launch of fast and slow asteroids from your launchers. Single-click a launcher to send a slow asteroid, after which your launcher will have a short reload time before it can launch again. Double-click a launcher to send a fast asteroid, but note that it will take twice as long to reload that launcher.

Each launcher has six asteroids loaded. Destroy the earth before you run out of asteroids and you win!
