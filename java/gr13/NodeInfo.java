package gr13;
/**********************************************************
 *	18748 - Spring 2015 - Group 13 
 *	Madhav Iyengar
 *	Miguel Sotolongo
 *	Nathaniel Jeffries
 **********************************************************/
// JSON Parsing
import org.json.*;
 
/**
 * NodeInfo
 */
public class NodeInfo {
	/***********************************************
	 * 	Final							   
	 ***********************************************/
	final String format = "--- MAC %d---\n" + 
	"light=%d   temp=%d \n" + 
	"acc_x=%d   acc_y=%d   acc_z=%d\n\n";
	
	/***********************************************
	 * 	Parameters							   
	 ***********************************************/
	// Unique ID
	public int mac;
	
	// Sensor Values
	public int light;
	public int temp;
	public int acc_x;
	public int acc_y;
	public int acc_z;
	
	public NodeInfo(int mac, String json) {
		this.mac = mac;
		getNodeInfo(json);
	}
	
	/// Parse JSON
	// 	Parses a JSONObject and fills NodeInfo values 
	private void getNodeInfo(String j) {
		try {
			JSONObject json = new JSONObject(j);
			// Parse Sensor Values (sample application)
			if(json.has("light"))
				light = json.getInt("light");
			if(json.has("temp"))
				temp  = json.getInt("temp");
			if(json.has("acc_x"))
				acc_x = json.getInt("acc_x");
			if(json.has("acc_x"))
				acc_y = json.getInt("acc_y");
			if(json.has("acc_z"))
				acc_z = json.getInt("acc_z");
			
		} catch (JSONException e) {
			System.err.println("Error: Failed to parse JSON");
			e.printStackTrace();
		}
	}
	
	/// Update
	//	Updates node info with new JSON
	public void update(String json) {
		getNodeInfo(json);
	}
	
	public String toString(){
		return String.format(format, 
			mac, light, temp, acc_x, acc_y, acc_z);
	}
}
