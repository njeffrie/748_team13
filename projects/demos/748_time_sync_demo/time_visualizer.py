import serial
import sys
import time

import pygtk
pygtk.require('2.0')
import gtk

import threading

class DisplayGui:
	def update_gui(self, wedget, data=None):
		print "updating gui"
	
	def delete_event(self, widget, event, data=None):
		print "delete event occurred"
		return False

	def destroy(self, widget, data=None):
		print "destroy signal occurred"
		gtk.main_quit()

	def drawTimerBar(self, data=None):
		self.area.window.draw_rectangle(self.gc, True, 10, 10, 80, 90)
		self.pangolayout.set_text("Rectangles")
		self.area.window.draw_layout(self.gc, 10, 10, self.pangolayout)
		return

	def __init__(self):
		self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
		self.window.connect("delete_event", self.delete_event)
		self.window.connect("destroy", self.destroy)
		self.area = gtk.DrawingArea()
		self.area.set_size_request(400, 300)
		self.pangolayout = self.area.create_pango_layout("")
		self.area.show()
		self.window.show()
	
	def main(self):
		gtk.main()

if __name__ == "__main__":
	hello = DisplayGui()
	hello.main()

	hello.drawTimerBar()
"""
ser = serial.Serial(sys.argv[1])
ser.baudrate = 9600;
curtime = int(round(time.time() * 1000)) #miliseconds
filename = '{0}{1}'.format("sensor_logs/sensor_log_", curtime); 
print(filename)
f = open(filename, 'wb')
print ser.name
print("set up serial");

class App(threading.Thread):
	def __init__(self):
		threading.Thread.__init__(self)
		self.start()

	def run(self):
		#self.root = tk.Tk()
		#label = tk.Label(self.root, text="Hello World")
		#label.pack()
		#self.root.mainloop()
		print "thread running"
		sys.exit(0)

app = App()

while 1:
  line = ser.readline()
  sys.stdout.write(line)
  f.write(line)
  """
