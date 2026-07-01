#!/usr/bin/python
'''
Requirement of gatt-client.py
	1. python 2 
	2. "pexpect" framework of python(using "sudo pip install pexpect") 
	3. gatttool & hcitool installed on host machine
	4. BT-enabled telo device
    5. One bluetooth dongle attached on Linux desktop(Host machine) 

process to execute :
	1. (mandatory)edit gatt-client.py 
		with DEVICE="YOUR-TELO_DEEVICE_BT_CHIP_MACID"
	2. (optional) to add new request edit "reqArray" to accomodate it
	3. cmd to use
		python gatt-client.py 

sample output

	Telo address:28:C2:DD:A0:33:A0
	Run gatttool...
	Connecting...
	Connected!
	setting MTU to 512
	MTU was exchanged successfully
	getting handle...
	Handle for this session: 0x002a
	Enabling notifications on handles : 0x0009, 0x0025, 0x002b


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 1, "tid" : 1 }
	char-write-req 0x002a 7b202272657122203a20312c202274696422203a2031207d00
	Response: { "req": 1, "tid": 1, "resp": { "code": 200, "data": { "version": 1 } } }


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 2, "tid" : 2 }
	char-write-req 0x002a 7b202272657122203a20322c202274696422203a2032207d00
	Response: { "req": 2, "tid": 2, "resp": { "code": 200, "data": { "myxid": "myx_00186134BA94" } } }


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 3, "tid" : 3 }
	char-write-req 0x002a 7b202272657122203a20332c202274696422203a2033207d00
	Response: { "req": 3, "tid": 3, "resp": { "code": 200, "data": { "name": "TeloAir" } } }


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 4, "tid" : 4 }
	char-write-req 0x002a 7b202272657122203a20342c202274696422203a2034207d00
	Response: { "req": 4, "tid": 4, "resp": { "code": 200, "data": { "version": 449258 } } }


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 10, "tid" : 1 }
	char-write-req 0x002a 7b202272657122203a2031302c202274696422203a2031207d00
	Response: { "req": 10, "tid": 1, "resp": { "code": 200, "data": { "count": 1 } } }
	

	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 12, "tid" : 8, "data" : { "index" : 0 } }
	char-write-req 0x002a 7b202272657122203a2031322c202274696422203a20382c20226461746122203a207b2022696e64657822203a2030207d207d00
	Response: { "req": 12, "tid": 8, "resp": { "code": 200, "data": { "ssid": "ooma_extreme", "security": "psk", "rssi": 85 } } }
	disconnected
	Exiting program

	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 19, "tid" : 19 }
	char-write-req 0x0d92 7b202272657122203a2031392c202274696422203a203139207d00
	Response: { "req": 19, "tid": 19, "resp": { "code": 200, "data": { "file1": "log", "file2": "locallog.5", "file3": "locallog.4", "file4": "locallog.3" } } }


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 20, "tid" : 20, "data" : { "selected_file" : "locallog.3" } }
	char-write-req 0x0d92 7b202272657122203a2032302c202274696422203a2032302c20226461746122203a207b202273616c7a5f6c6f675f66696c6522203a20226c6f63616c6c6f672e3322207d207d00
	Response: { "req": 20, "tid": 20, "resp": { "code": 200, "data": { "timestamp": "2020-04-29 06:17:27 -0700\n", "log_lines": 2205 } } }


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 21, "tid" : 21, "data" : { "line": 1} }
	char-write-req 0x0f66 7b202272657122203a2032312c202274696422203a2032312c20226461746122203a207b20226c696e65223a20317d207d00
	Response: { "req": 21, "tid": 21, "resp": { "code": 200, "data": { "line1": "1. This is the first log line.\n" } } }


	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	sending data: { "req" : 22, "tid" : 22, "data" : { "selected_file" : "locallog.3" } }
	char-write-req 0x0d9e 7b202272657122203a2032322c202274696422203a2032322c20226461746122203a207b202273616c7a5f6c6f675f66696c6522203a202264612e736122207d207d00
	Response: { "req": 22, "tid": 22, "resp": { "code": 200 } }

'''

import pexpect
from time import sleep
DEVICE = "A8:1D:16:82:72:7A"
# DEVICE = "28:C2:DD:A0:33:A0"

char_uuid = "ad945229-f59f-4808-a382-10b99be092e3"
char_uuid_clear_text = "c2921afe-f369-49fa-91cf-2760fd66e033"

def to_hex(value):
	value = value.replace(" ", "")
	return value.replace("00", "20")


def hex_to_string(hexstring):
	return hexstring.decode('hex')


def string_to_hex(string):
	return string.encode('hex')

 
def printDebudChild():
	global child
	print("value of child.after: ")
	print(child.after)
	print("value of  child.before: ")
	print(child.before)
	print("value of child.buffer: ")
	print(child.buffer)

# write data
def write_to_handle(handle, value):
	print '\n\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>'
	print 'sending data: ' + value
	global child
	cmd = "char-write-req " + handle + " " + string_to_hex(value) + "00\r"
	print cmd
	child.send(cmd)
	child.expect("successfully")


# read data
def read_from_handle(handle):
	global child
	cmd = "char-read-hnd " + handle + " \r"
	print cmd
	child.send(cmd)
	child.expect("Characteristic value/descriptor: ")
	child.expect("\r\n")
	spaced_hex_string = child.before
	return hex_to_string(to_hex(spaced_hex_string))

def read_from_notif(handle):
	global child
	child.expect('Notification handle = ' + handle +  ' value: ')
	spaced_hex_string = child.buffer
	spaced_hex_string = spaced_hex_string.split("\n")[0]
	return hex_to_string(to_hex(spaced_hex_string.strip()))

def clean_up():	
	global child
	child.sendline("disconnect")
	print 'disconnected'
	child.sendline("exit")
	print 'Exiting program'


# Run gatttool interactively.
print("Telo address:" + DEVICE)
print("Run gatttool...")
child = pexpect.spawn("gatttool -i hci0 -b " + DEVICE + " -I")

 
# Connect to the device.
print("Connecting...")
child.sendline("connect")
child.expect("Connection successful", timeout=10)
print("Connected!")


# change MTU
print("setting MTU to 512")
child.sendline("mtu 512")
child.expect("MTU was exchanged successfully")
print("MTU was exchanged successfully")


# Get handle for char_uuid
print("getting handle...")
child.sendline("characteristics")
child.expect(" uuid: " + char_uuid_clear_text)
handle = child.before[-7:-1]
print "Handle for this session: " + handle


#get handle for notifications
child.sendline('char-read-uuid 2902');
child.expect('handle: ')

handleStr = 'handle: '
notifHandle1 = child.buffer[:6]
temp = child.buffer[child.buffer.find(handleStr)+len(handleStr):]
notifHandle2 = temp[:6]
temp = temp[temp.find(handleStr)+len(handleStr):]
notifHandle3 = temp[:6]


print("Enabling notifications on handles : " + notifHandle1 + ', ' +notifHandle2 + ', ' +notifHandle3)
child.sendline('char-write-req ' + notifHandle1 + ' 0100')
child.sendline('char-write-req ' + notifHandle2 + ' 0100')
child.sendline('char-write-req ' + notifHandle3 + ' 0100')



value1 = '{ "req" : 1, "tid" : 1 }'
value2 = '{ "req" : 2, "tid" : 2 }'
value3 = '{ "req" : 3, "tid" : 3 }'
value4 = '{ "req" : 4, "tid" : 4 }'
value5 = '{ "req" : 10, "tid" : 1 }'
value6 = '{ "req" : 12, "tid" : 8, "data" : { "index" : 0 } }'
# req-19: get_file_name_list
value19 = '{ "req" : 19, "tid" : 19 }'
# req-20: select_file
value20 = '{ "req" : 20, "tid" : 20, "data" : { "selected_file" : "locallog.3" } }'
# req-22: delete_file
value22 = '{ "req" : 22, "tid" : 22, "data" : { "selected_file" : "locallog.3" } }'
# req-23: put_file: Yet to be implemented
value23 = '{ "req" : 23, "tid" : 23 }'

reqArray = [value1, value2, value3, value4, value5, value6]
reqArray = [value19, value20]
reqArray = [value20]
reqArray = []

for data in reqArray:
	write_to_handle(handle, data)
	sleep(1)
	print 'Response: ' + read_from_notif(handle)
	sleep(1)


# TR-1559:
# 	Uncomment the function call to test the file handling APIs

for req in [value19, value20, value22, value19, value23]:
	write_to_handle(handle, req)
	sleep(1)
	print 'Response: ' + read_from_notif(handle)
	sleep(1)

def read_file():
	tid = 21
	no_of_log_lines = 25			# set this dynamically from response received to req-20
	line = 1
	while line <= no_of_log_lines:
		# req-21: get_line
		req = '{ "req" : 21, "tid" : ' + str(tid) + ', "data" : { "line": '+ str(line)+ '} }'
		write_to_handle(handle, req)
		# sleep(.1)
		print 'Response: ' + read_from_notif(handle) + '\n'
		tid = tid + 1
		line = line + 1

# read_file()
# End of TR-1559

clean_up()