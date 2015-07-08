FusionWM - Fusion between HerbstluftWM and DynamicWM
====================================================

Difference from HWM:
--------------------
  - No hooks
  - No configuration files (only configuration headers)
  - Xinerama (multi-head support)
  - Native bar
  - Frame management:
   - Floating windows do not occupy frame space (unlike pseudotile under hwm)
   - Automatically remove frame if last window in the frame is removed
   - Automatically move the last window to the new frame when frame is split

Difference from DWM:
--------------------
  - Manual tiling
  - Better code organization
  - Exclusive number of tags: the defined number of tags don't double if you attach an external monitor

Other differences and features:
-------------------------------
  - Native systray

A Screenshot:
-------------
<a href='http://s6.postimg.org/gtajwnv8h/screenshot.png' width=300><img src='http://s6.postimg.org/gtajwnv8h/screenshot.png' /></a>

Version Log:
------------
  - 1.0 (2015-06-30): Adding native systray
  - 0.4 (2012-04-18): Experimental stages

To Do:
------
  - BUG: When floating client is outside the current monitor, mouse cannot follow
  - WISHLIST: Code cleanup
