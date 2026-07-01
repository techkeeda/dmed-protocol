bleclient can build built natively on Ubuntu hosts with some preparation

You need to have dependencies installed, like this:
sudo apt install -y openssl  libjson-c-dev libdbus-glib-1-dev liblua5.2-dev libluabind-dev

Then build like this:
make clean && make -f Makefile.native 

For now, you will need to edit Makefile.native to switch over to ARM64 setup, like fermisur or clearfog

