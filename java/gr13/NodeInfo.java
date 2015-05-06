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
	"pres=%d\nlt=%d\ngt=%d tdma_slot=%d\n\n";
	final int oval_width = NetworkGraph.GRID_DIM/2;
	final int oval_height = NetworkGraph.GRID_DIM/2;
	
	// Timing Constants
	final long NEW_TIMEOUT  = 50; // Green color timeout
	final long IDLE_TIMEOUT = 3000; // Yellow color timeour
	final int DRIFT_MAX = 500; // Max drift allowed
	final int DRIFT_GRAPH_MAX = 300; // Pixel limit of drift graph
	
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
	public int tdma_slot;
	
	// TimeStamps
	public long lt = 0; // Local Time
	public long gt = 0; // Global Time
	public long last_update;
	long avg_drift = 0;
	int avg_count = 0;
	
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
			if(json.has("slot"))
				tdma_slot = json.getInt("slot");
			
			// Parse Time Stamps
			if(json.has("lt"))
				lt = json.getLong("lt");
			if(json.has("gt"))
				gt = json.getLong("gt");
			if(json.has("lt") && json.has("gt")) {
				avg_count++;
				avg_drift += lt - gt;
			}
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
			mac, pres, lt, gt, tdma_slot);
	}
	
	/******************************************************
	* 	Draw Functions
	******************************************************/
	public void draw(Graphics g, long timestamp) {
		long elapse_time = timestamp - last_update;
		if(elapse_time <= NEW_TIMEOUT)
			g.setColor(Color.green);
		else if(elapse_time > NEW_TIMEOUT && elapse_time <= IDLE_TIMEOUT)
			g.setColor(Color.yellow);
		else
			g.setColor(Color.red);	
		
		// Draw Node
		drawOval(g, 
			grid_x * NetworkGraph.GRID_DIM + 50,
			grid_y * NetworkGraph.GRID_DIM + 50, 
			Integer.toString(mac)
		);
	}
	
	public void drawText(Graphics g) {
		g.setColor(Color.black);
		g.drawString(String.format("pressure: %d", pres), 
		grid_x * NetworkGraph.GRID_DIM + 55,
		grid_y * NetworkGraph.GRID_DIM + 85);
		g.drawString(String.format("timestamp: %d", lt), 
		grid_x * NetworkGraph.GRID_DIM + 55,
		grid_y * NetworkGraph.GRID_DIM + 95);
	}
	
	public void drawLine(Graphics g, int x, int y) {
		g.setColor(Color.black);
		g.drawLine(grid_x * NetworkGraph.GRID_DIM + 50, 
				   grid_y * NetworkGraph.GRID_DIM + 50,
				   x * NetworkGraph.GRID_DIM + 50, 
				   y * NetworkGraph.GRID_DIM + 50);
	}
	
	public void drawDrift(Graphics g) {
		// Lots of assumptions are made for this method:
		// 1. There is only one node in the NetworkGraph
		// 2. It is located on coordinate (3,0)
		Graphics2D g2 = (Graphics2D) g;
		
		// Calculate graph scale
		long drift = lt - gt;
		double ratio = (drift >= DRIFT_MAX) ? 1 : ((double) Math.abs(drift)) / DRIFT_MAX; 
		int scaling_param = (int)(ratio * DRIFT_GRAPH_MAX);

		int g_s_x, g_s_y; // Gradient P1
		int g_e_x, g_e_y; // Gradient P2
		int r_s_x, r_s_y; // Bar P1
		int r_w, r_h; 	  // Bar width, height
		int t_x, t_y;	  // Text P1
		if(drift >= 0) {
			g_s_x = grid_x * NetworkGraph.GRID_DIM + 50;
			g_s_y = (grid_y + 1) * NetworkGraph.GRID_DIM + NetworkGraph.GRID_DIM/2;
			g_e_x = grid_x * NetworkGraph.GRID_DIM + 50 + DRIFT_GRAPH_MAX;
			g_e_y = (grid_y + 1) * NetworkGraph.GRID_DIM + NetworkGraph.GRID_DIM/2;
			r_s_x = g_s_x;
			r_s_y = g_s_y;
			r_w   = scaling_param;
			r_h   = NetworkGraph.GRID_DIM;
			t_x   = grid_x * NetworkGraph.GRID_DIM - 75;
			t_y   = (grid_y + 2) * NetworkGraph.GRID_DIM;
		} else {
			g_s_x = grid_x * NetworkGraph.GRID_DIM + 50;
			g_s_y = (grid_y + 1) * NetworkGraph.GRID_DIM + NetworkGraph.GRID_DIM/2;
			g_e_x = grid_x * NetworkGraph.GRID_DIM + 50 - DRIFT_GRAPH_MAX;
			g_e_y = (grid_y + 1) * NetworkGraph.GRID_DIM + NetworkGraph.GRID_DIM/2;
			r_s_x = grid_x * NetworkGraph.GRID_DIM + 50 - scaling_param;
			r_s_y = g_s_y;
			r_w   = scaling_param;
			r_h   = NetworkGraph.GRID_DIM;
			t_x   = grid_x * NetworkGraph.GRID_DIM + 125;
			t_y   = (grid_y + 2) * NetworkGraph.GRID_DIM;
		}
		
		// Draw
		GradientPaint bluetored = new GradientPaint(
			g_s_x, g_s_y, Color.yellow,
			g_e_x, g_e_y, Color.red
		);
		g2.setPaint(bluetored);
		g2.fill(new Rectangle(r_s_x, r_s_y, r_w, r_h));
		g2.setPaint(Color.black);
		
		// Local Time
		g2.setFont(new Font("TimesRoman", Font.PLAIN, 16)); 
		g2.drawString(String.format("lt: %d", lt), t_x, t_y);
		// Global Time
		g2.drawString(String.format("gt: %d drift: %d", gt, lt-gt), 
			grid_x * NetworkGraph.GRID_DIM + 75, 
			(grid_y + 3) * NetworkGraph.GRID_DIM);
		g2.drawString(
			String.format("average drift: %d", avg_drift/avg_count), 
			grid_x * NetworkGraph.GRID_DIM + 75, 
			(grid_y + 3) * NetworkGraph.GRID_DIM + 15);
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