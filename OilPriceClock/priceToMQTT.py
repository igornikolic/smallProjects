#!/usr/bin/python3
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import requests

opecData = False

#The data
#https://www.quandl.com/data/OPEC/ORB-OPEC-Crude-Oil-Price

r = requests.get('https://www.quandl.com/api/v3/datasets/OPEC/ORB.json?limit=1&api_key=XXXXXXXXXXXXX')
jsonFile = r.json()
#dataset contain the price information. JSON is a dict, and'data' is a list, that contains a dict
dataset = jsonFile['dataset']
#we want the latest price known; if there is no trade, the last will be "-", so we need to catch that
data = dataset['data']
lastEntry = data[0]
OPECPrice = float(lastEntry[1])

r = requests.get('https://www.cmegroup.com/CmeWS/mvc/Quotes/Future/425/G?quoteCodes=CL')
jsonFile = r.json()
lastEntry = jsonFile['quotes'][0]
futuresPrice = float(lastEntry['last'])


#the change compared to the previous days closing price - currently not calculating or displaying it.


#Prepare the  mqtt stuff
priceTopicOPEC  = "revspace/OPECOilPrice"
priceTopicFutures = "revspace/oilFuturesPrice"

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
msgs = [{'topic':priceTopicOPEC,'payload':OPECPrice,'qos':2,'retain':False},
    (priceTopicFutures, futuresPrice, 2, False)]

#publish and forget
publish.multiple(msgs, hostname)

