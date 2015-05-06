package gr13;
/**********************************************************
 *	18748 - Spring 2015 - Group 13 
 *	Madhav Iyengar
 *	Miguel Sotolongo
 *	Nathaniel Jeffries
 **********************************************************/

// Serial Connection
import jssc.SerialPort;
import jssc.SerialPortList;
import jssc.SerialPortException;

// Input Streams 
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;	

// JSON Parsing
import org.json.*;

/**
 * SerialComm
 * Handles SerialComm with NanoRK Gateway node.
 */
public class SerialComm {
	/***********************************************
	 * 	Constants							   
	 ***********************************************/
	private final int READ_PERIOD = 50;
	private final int WRITE_PERIOD = 10;
	private final int BUFFER_SIZE = 2048;
	
	JGui gui;
	
	void connect(String port, JGui gui) throws Exception {
		System.err.println("Status: Reading from Port " + port);
		
		try{
			SerialPort comPort = new SerialPort(port);
			comPort.openPort();
			comPort.setParams(
				SerialPort.BAUDRATE_115200, 
				SerialPort.DATABITS_8, 
				SerialPort.STOPBITS_1,
				SerialPort.PARITY_NONE
			);
				
			this.gui = gui;
			(new Thread (new SerialReader(comPort))).start();
			(new Thread (new SerialWriter(comPort))).start();
		} catch (Exception e) {
			System.err.println("Error: Port unavailable.");
			e.printStackTrace();
		}

		
	}
	
	/// SerialReader
	//	Reads from USB port every 10ms
	public class SerialReader implements Runnable {
		SerialPort in;
		
		/// Constructor
		public SerialReader(SerialPort in) {
			this.in = in;
		}
		
		/// Thread 
		public void run() {
			String[] read;
			JSONObject parser;
			byte[] buf;
			String[] json;
			
			try 
			{
			while(true){
				// Read from ComPort
				buf = in.readBytes();
						
				// Display and interpret input 
				if(buf != null){
					System.out.print("Read: " + new String(buf));
					read = new String(buf).split("\r\n");	
					for(String r : read) {
						if(r != null && isJSONValid(r)){	
							parser = new JSONObject(r);		
							
							// Extract NodeInfo
							NodeInfo info = new NodeInfo( 
								parser.getInt("mac"),
								r);
							gui.addNode(info.mac, info);
						} 
					}
							
				}
						
				// Sleep for READ_PERIOD
				Thread.sleep(READ_PERIOD);
			}	
			} catch (Exception e) {
				System.err.println("Error: Read");
				e.printStackTrace();
			}
		}
		
		///isJSONValid
		// Returns true if json is a valid JSONObject
		public boolean isJSONValid(String json){
			try {
				new JSONObject(json);
			} catch(JSONException ex) {
				return false;
			}
			return true;
		}
		
	}
	
	public class SerialWriter implements Runnable {
		SerialPort out;
		
		public SerialWriter(SerialPort out) {
			this.out = out;
		}
		
		public void run() {
			try{
				int c = -1;
				while((c = System.in.read()) > -1)
				{
					System.out.println("Sending: " + c);
					out.writeInt(c);
				}	
				
				// Sleep for WRITE_PERIOD
				Thread.sleep(WRITE_PERIOD);
			} catch (Exception e) {
				System.err.println("Error: Write");
				e.printStackTrace();
			}
		}
		
	}
}