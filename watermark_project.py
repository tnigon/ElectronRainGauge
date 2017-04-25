#!/usr/bin/python

# This code is using a University of Cambridge's code example of OpenLabTools and Adafruit's libraries for the ADC converter(ADS1115)

from Adafruit_ADS1x15 import ADS1x15
import time
import numpy as np
import sys

pga= 4096                 #(page 13 datasheet ADS1115 ), change depending on the input voltage. Voltage input in this case is 3300 mV so the pga is 40$
sps= 8                    #sps(samples per seconds see datasheet page 13), how many samples the ADC will take to give a reading or meaurement. 8 becau$
ADS1115= 0x01             #it specifies that the device being used is the ADS1115 (Adafruit library requirement, because there are more ADC converters)
adc= ADS1x15(ic=ADS1115)  #create and instance of the class ADS1x15 called adc 

# Function to save data to a file (in this case we are using numpy savetxt function as a means to save data to a file) 

def logdata():
    numRows = 2
    dataarray=np.zeros([numRows,3]) #create a numpy array (3 columns and 15 rows) at which measurements are taken (0=coordinates, 1=tension, 2=date and time)

    for x in range(0, numRows):
        time.sleep(1)
        dataarray[x, 2] = time.strftime("%m%d%Y%H%M%S") #specifies date: month(00), day(00), year(0000), hour(00), minute(00), second(00). it uses 24 hours' format  
        dataarray[x, 1] = ((0.083*(adc.readADCSingleEnded(3, pga, sps)))-0.323) #take a sample from A3 input on the ADS1115  and convert the value fro$
        dataarray[x, 0] = 123456 #please define coordinates for this device instead of 123456

    return(dataarray)

datasamples = logdata() #call function to log data

f_handle = file(sys.argv[1]+".csv", 'a')    #get the handle for the file.
np.savetxt(f_handle, datasamples, fmt='%3.f', delimiter=',') #header='tension(cbar),date,hour') #numpy function to save data i$
f_handle.close();     #close the file handle.

#the name of this file will be the date and time at which measurements were taken, and they will be stored in this directory /home/pi/Adafruit-Raspberry-Pi-Python-Code/Adafruit_ADS1x15
#syntax of numpy.savetxt(fname, X, fmt='%18e', delimiter='') 
#where fname is the file name, X is the data to saved to a text file, delimiter is the string o character separating columns. 
