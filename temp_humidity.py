"""
My First Internet of Things

Temperature/Humidity Light monitor using Raspberry Pi, DHT11, and photosensor 
Data is displayed at thingspeak.com
2015/06/15
SolderingSunday.com

Based on project by Mahesh Venkitachalam at electronut.in

"""

# Import all the libraries we need to run
import sys
import RPi.GPIO as GPIO
import os
from time import sleep
import Adafruit_DHT
import urllib2


DEBUG = 1
# Setup the pins we are connect to
RCpin = 24
DHTpin = 23

#Setup our API and delay
myAPI = "ZTPH2UW6F3ZQ7WVN"
postTimeDelay = 1 #how many seconds between posting data

GPIO.setmode(GPIO.BCM)
GPIO.setup(RCpin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)


def getSensorData():
    humidity, tempC = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, DHTpin)
    
    #Convert from Celius to Farenheit
    tempF = 9/5*tempC+32
   
    # return dict
    return (str(humidity), str(tempC), str(tempF))

def RCtime(RCpin):
    LT = 0
    
    if (GPIO.input(RCpin) == True):
        LT += 1
    return (str(LT))
    
    
# main() function
def main():
    
    print 'starting...'

    baseURL = 'https://api.thingspeak.com/update?api_key=%s' % myAPI
    print baseURL
    
    while True:
        try:
            humidity, tempC, tempF = getSensorData()
            #LT = RCtime(RCpin)
            #f = urlli2.urlopen(baseURL + 
            #                    "&field1=%s&field2=%s&field3=%s" % (tempC, tempF, humidity)+
            #                    "&field4=%s" % (LT))

            f = urllib2.urlopen(baseURL + 
                                "&field1=%s & field2=%s" % (tempC, humidity))
            print f.read()

            # Celsius
            print "Temp: " + tempC + "C " + " Humidity: " + humidity + "%"

            #Fahrenheit
            #print "Temp: " + tempF + "F " + RHW + " " + LT
            
            f.close()
            
            sleep(int(postTimeDelay))
            
        except:
            print 'exiting.'
            break

# call main
if __name__ == '__main__':
    main()
