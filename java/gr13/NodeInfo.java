package gr13;
/**********************************************************
 *	18748 - Spring 2015 - Group 13 
 *	Madhav Iyengar
 *	Miguel Sotolongo
 *	Nathaniel Jeffries
 **********************************************************/
// JSON Parsing
import org.json.*;

// Draw
import java.awt.*;
 
/**
 * NodeInfo
 */
public class NodeInfo {
	/***********************************************
	 * 	Final							   
	 ***********************************************/
	final String format = "--- MAC %d---\n" + 
	"pres=%d\nlt=%d\ngt=%d\n\n";
	final int oval_width = NetworkGraph.GRID_DIM/2;
	final int oval_height = NetworkGraph.GRID_DIM/2;
	
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
	public int pres;
	
	// TimeStamps
	public long lt = 0; // Local Time
	public long gt = 0; // Global Time
	public long last_update;
	
	// Graph Info
	public int grid_x;
	public int grid_y;
	public String json; // JSON String from SerialComm 
	
	public NodeInfo(int mac, String json) {
		this.mac = mac;
		this.json = json;
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
			if(json.has("pres"))
				pres = json.getInt("pres");
			
			// Parse Time Stamps
			if(json.has("lt"))
				lt = json.getLong("lt");
			if(json.has("gt"))
				gt = json.getLong("gt");
		} catch (JSONException e) {
			System.err.println("Error: Failed to parse JSON");
			e.printStackTrace();
		}
	}
	
	/// Update
	//	Updates node info with new JSON
	public void update(String json) {
		this.last_update = System.currentTimeMillis();
		this.json = json;
		getNodeInfo(json);
	}
	public void update() {
		this.last_update = System.currentTimeMillis();
		getNodeInfo(json);
	}
	
	public String toString(){
		return String.format(format, 
			mac, pres, lt, gt);
	}
	
	//// Draw
	public void draw(Graphics g, long timestamp) {
		long elapse_time = timestamp - last_update;
		if(elapse_time <= 1000)
			g.setColor(Color.green);
		else if(elapse_time > 1000 && elapse_time <= 4000)
			g.setColor(Color.yellow);
		else
			g.setColor(Color.red);	
		
		// Draw Node
		drawOval(g, 
			grid_x * NetworkGraph.GRID_DIM + 50,
			grid_y * NetworkGraph.GRID_DIM + 50, 
			Integer.toString(mac)
		);
		g.setColor(Color.black);
		g.drawString(String.format("drift: %d", lt - gt), 
			grid_x * NetworkGraph.GRID_DIM + 55,
			grid_y * NetworkGraph.GRID_DIM + 85);
	}
	
	public void drawLine(Graphics g, int x, int y) {
		g.setColor(Color.black);
		g.drawLine(grid_x * NetworkGraph.GRID_DIM + 50, 
				   grid_y * NetworkGraph.GRID_DIM + 50,
				   x * NetworkGraph.GRID_DIM + 50, 
				   y * NetworkGraph.GRID_DIM + 50);
	}
	
	private void drawOval(Graphics g, int centerX, 
						  int centerY, String text) 
	{	
		// Draw Oval
		g.fillOval(
			centerX-this.oval_width/2, centerY-this.oval_height/2, 
			oval_width, this.oval_height
		);
		
		g.setColor(Color.white);
		// Draw text
        FontMetrics fm = g.getFontMetrics();
        double textWidth = fm.getStringBounds(text, g).getWidth();
        g.drawString(text, (int) (centerX - textWidth/2),
                    (int) (centerY + fm.getMaxAscent() / 2));
	}
	
	
}