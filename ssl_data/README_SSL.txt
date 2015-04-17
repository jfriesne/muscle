The .pem files in this folder are pre-generated OpenSSL private and public
keys, to give you an example for what the key files should look like.
Don't use them for anything serious, since the the private key has been
distributed to the world and thus it is not very private anymore.  :)

To Generate your own self-signed private key and matching public
certificate, execute the following shell commands:

   openssl req -x509 -days 3650 -nodes -newkey rsa:1024 -keyout my_private_key.pem -out my_public_key.pem
   cat my_public_key.pem >> my_private_key.pem

To test/verify MUSCLE's OpenSSL support under Linux or MacOS/X, do the
following:

 - In your Terminal window:
   - Under Linux only, make sure package libssh-dev is installed
     (MacOS/X has it installed by default)
   - Edit muscle/server/Makefile and uncomment the line
        CXXFLAGS += -DMUSCLE_ENABLE_SSL
   - Compile muscled: cd muscle/server; make clean; make
   - Run muscled:
        ./muscled displaylevel=trace privatekey=../ssl_data/muscle_test_private_key.pem

 - In a second Terminal window:
   - Edit muscle/test/Makefile and uncomment the line
        CXXFLAGS += -DMUSCLE_ENABLE_SSL line
   - cd ../test; make clean; make
   - Run portablereflectclient:  ./portablereflectclient localhost publickey=../ssl_data/muscle_test_public_key.pem
   - portablereflectclient should connect to muscled and stay connected.
     It should mention on stdout that it is using the specified public key
     file to connect to the server.
   - enter P (or L) and press return.  If you get a bunch of parameter data
     printed in response, then the SSL connection is working.

 - (Optional, if you like Qt-based GUI demos) In a third Terminal window
   - Make sure a Qt 4.x or Qt 5.x development environment is installed on your system.
   - cd ../qtsupport/qt_example ; touch ./muscle_enable_ssl
   - qmake; make clean; make
   - ./qt_example.app/Contents/MacOS/qt_example publickey=../../ssl_data/muscle_test_public_key.pem
     (or under Linux, it's:  ./qt_example publickey=../../ssl_data/muscle_test_public_key.pem )
   - In the window that appears, click the "Connect to Server" button if
     necessary
   - Click the "Clone" button to  create a second window
   - If everything is working, dragging in one window should show up in the
     second window and vice versa.
   - Click the "Animate" checkbox in one or both windows to animate positions.

