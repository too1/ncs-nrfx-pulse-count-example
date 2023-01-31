**ncs-nrfx-pulse-count-example**

Overview
********
Simple example showing how to use the nrfx_gpiote, nrfx_gppi and nrfx_timer drivers to implement a simple pulse counter mechanism. 
Will set up one GPIOTE channel in IN mode, and connect the event to the count task of a timer over PPI/DPPI. 


Requirements
************

- nRF Connect SDK v2.2.0
- Tested on the following board(s);
	- nrf52840dk_nrf52840
	
TODO
****
- Test on a device using DPPI rather than PPI (like the nRF5340)

