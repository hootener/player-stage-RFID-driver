player-stage-RFID-driver
========================

A driver that can be used to read RFID tags within the PlayerStage robot platform. It will probably need to be modified to support your particular application, but it's a good start.

This particular driver was designed to work with the GAO RFID 433 MHz Active RFID reader with RS232 connection. This reader can be found at (http://www.gaorfid.com/index.php?main_page=product_info&cPath=133&products_id=669). However, with modification, this driver can probably work with any RFID reader.

If you're using a different reader/tag, at the very least you will need to modify the protocol used to parse the
information obtained from reading the tags. 

The build process for this driver has only been tested under Linux.

Compiling / Building / Testing
-------------------------------

This driver can be built using CMake, and contains a CMakeLists file for building. The compile script can be executed by typing "bash compile" from the command line. It will recreate the contents of the build directory using CMake. If you make changes to the RFIDdriver.cpp or .h files, save them, and then call bash
compile to rebuild the rebuild library. The compile script will also launch Player using the provided testrfid.cfg file, which tests the RFID reader. Obviously, you will need to have the reader connected to the running platform.
