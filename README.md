# self-water-planter

This project is a self-watering planting system using a [Seeed Wio Terminal](https://www.seeedstudio.com/Wio-Terminal-p-4509.html). You can find some photos at the [blog post](http://lindsayrgwatt.com/blog/2021/02/plants-without-the-effort/) I wrote up about the project.

Here's the sensor & compute BOM:

* Computer: [Seeed Wio Terminal](https://www.seeedstudio.com/Wio-Terminal-p-4509.html)
* Water level sensor: [Seeed Grove Water Level Sensor](https://www.seeedstudio.com/Grove-Water-Level-Sensor-10CM-p-4443.html)
* Capacitive moisture sensor: [DFRobot Gravity Analog Capacitive Soil Moisture Sensor](https://www.dfrobot.com/product-1385.html)
* Relay: [Seeed Grove Relay](https://www.seeedstudio.com/Grove-Relay.html)

Other physical items:

* I pulled the pump from an old kit; if you search for "water pump" on Seeed or DFRobot you'll find many options
* To distribute water I used an old piece of broken drip distribution tubing; just drilled some holes in it where I wanted water to come out faster
* To mount the Wio Terminal and and the water level sensor in place I 3D printed some brackets; the .stl files are part of this github repo

There are actually two Arduino sketches included

* `simple_self_watering_plant_system.ino` uses the full sensor/compute package but is stripped of any display code. Read this to see how the feedback loop between sensors and watering works. Pukes lots of debug info to serial
* `wio_terminal_self_watering_plant_system.ino` includes all the code for displaying info on the Wio Terminal, allows you to toggle LCD with top buttons and allows you to use joystick to adjust moisture level. Similar underlying feedback loop for watering execept that it includes a hystersis parameter as relay was thrashing on/off around desired moisture level (set to 0 if you want to see).

If you find this useful, let me know.
