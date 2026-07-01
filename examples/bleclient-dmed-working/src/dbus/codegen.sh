#!/bin/bash
# This script generated headers and c files based on .xml definitions
# Does not involve c compiler

if [ -z $TOPDIR ]; then 
	echo "Running codegen.sh for bleclient native build"
	CODEGEN=$(which gdbus-codegen)
	if [ -z $CODEGEN ]; then 
		echo "Error. gdbus-codegen not on path"
		exit 1
	fi
else
	# Set absolute path for multitarget build
	CODEGEN=$TOPDIR/staging_dir/host/bin/gdbus-codegen
fi

 

$CODEGEN  --generate-c-code adapter-generated adapter.xml

# authentication agent
$CODEGEN  --generate-c-code agent-generated agent.xml

# ObjectManager
$CODEGEN  --generate-c-code objectmanager-generated object-manager.xml

# codegen for profile implemented by application
$CODEGEN  --generate-c-code advertise-generated advertisement.xml
$CODEGEN  --generate-c-code lteservice-generated lteservice.xml
$CODEGEN  --generate-c-code service-generated service.xml
$CODEGEN  --generate-c-code characteristic-generated characteristic.xml \
	--annotate "org.bluez.GattCharacteristic1.WriteValue()[data]" org.gtk.GDBus.C.ForceGVariant data \
	--annotate "org.bluez.GattCharacteristic1:Value" org.gtk.GDBus.C.ForceGVariant value
$CODEGEN  --generate-c-code descriptor-generated descriptor.xml \
	--annotate "org.bluez.GattDescriptor1.ReadValue()[data]" org.gtk.GDBus.C.ForceGVariant data
