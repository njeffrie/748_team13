#######################################
#	Group 13 - JGui MakeFile		  #	
#######################################
DIR = gr13
JFLAGS = -g
JC = javac
JVM = java
JLOC = -sourcepath gr13

# Macro for each of the java source files.
CLASSES = \
	$(DIR)/NetworkGraph.java \
	$(DIR)/JGui.java \
	$(DIR)/SerialComm.java \
	$(DIR)/NodeInfo.java

# Dependencies in lib/ to load
 JDEP = \
 	lib/jssc.jar:lib/orgjson.jar

# Name of class with main function
MAIN = $(DIR)/JGui

default: classes

classes: 
	$(JC) $(JFLAGS) $(JLOC) -cp .:$(JDEP) $(CLASSES)

run: classes
	$(JVM) -classpath  .:$(JDEP) $(MAIN) 

clean:
	$(RM) $(DIR)/*.class
