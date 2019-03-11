#!/usr/bin/python3
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import requests
import xml.etree.ElementTree as ET


#The data
#

r = requests.get('https://www.tennet.org/xml/balancedeltaprices/balans-delta_2h.xml')
xmlfile = r.text
#print(xmlfile)
root = ET.fromstring(xmlfile)
up = 0
down = 0
price = 0
delta = 0

for record in root.iter('RECORD'):
	up = record.find('UPWARD_DISPATCH').text
	down = record.find('DOWNWARD_DISPATCH').text
	price = 0 # reset the price value
	priceRecord = record.find('MAX_PRICE')
	if 	priceRecord != None:
		price = priceRecord.text
	sequence = record.find('NUMBER').text
	#print(up, down, price)
	if sequence == '1':	
		#print('First record')
		#print(up, down, price)
		break

delta = int(up) - int(down)



#Prepare the  mqtt stuff
ballanceTopicUpwardDispatch  = "revspace/TennetBallansUpwardDispatch"
ballanceTopicDownwardDispatch  = "revspace/TennetBallansDownwardDispatch"
ballanceTopicDeltaDispatch  = "revspace/TennetBallansDeltaDispatch"
ballanceTopicPrice  = "revspace/TennetBallansPrice"



hostname = "mosquitto.space.revspace.nl"

'''
# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))
'''

#lets construct the message
msgs = [{'topic':ballanceTopicUpwardDispatch,'payload':up,'qos':2,'retain':False}, 
(ballanceTopicDownwardDispatch, down, 2, False),
(ballanceTopicPrice, price, 2, False),
(ballanceTopicDeltaDispatch, delta, 2, False)
]

#publish and forget
publish.multiple(msgs, hostname)

