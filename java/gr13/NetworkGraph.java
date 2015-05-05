package gr13;
/**********************************************************
 *	18748 - Spring 2015 - Group 13 
 *	Madhav Iyengar
 *	Miguel Sotolongo
 *	Nathaniel Jeffries
 **********************************************************/
import java.util.Random;
import java.awt.*;
import javax.swing.JPanel;


public class NetworkGraph extends JPanel implements Runnable {
	public static final int GRID_DIM = 100;
	final int GRID_WIDTH = 7; // Grid squares 
	final int GRID_HEIGHT = 7;
	final int FPS = 10;
	
	boolean grid[][];
	int free_slots;
	JGui gui;
	Random random;
	NodeInfo gateway;
	
	public NetworkGraph(JGui gui) { 
		this.gui = gui; 
		this.grid = new boolean[GRID_WIDTH][GRID_HEIGHT];
		random = new Random();
		
		// Manually add gateway node
		gateway = new NodeInfo(0, "");
		gateway.grid_x = 3;
		gateway.grid_y = 3;
		grid[3][3] = true; // Reserved for gateway node
		free_slots = 10;
	}
	
	/// run
	//  Updates all nodes and draws them
	public void run() {
		try {
		//HACK: There are better ways to do an update loop
		//		luckily we aren't rendering too much.
		while(true) {
			// Update Nodes
			gui.updateInformation();
			
			// Paint Components
			this.repaint();
			
			Thread.sleep(1000/FPS);
		}
		} catch (Exception e){
			System.err.println("Graph failed");
		} 
	}
	
	/// addNode
	// 	Sets a nodes X and Y coordinates
	public void addNode(NodeInfo node) {
		int counter = 0;
		boolean isPlaced = false;
		while(!isPlaced) {
			int r_row = random.nextInt(GRID_WIDTH  - 1);
			int r_col = random.nextInt(GRID_HEIGHT - 1);
			if(!grid[r_row][r_col]){
				grid[r_row][r_col] = true;
				isPlaced = true;
				node.grid_x = r_row;
				node.grid_y = r_col;
				free_slots--;
			}	
		}
	}
	
	/// Draw Methods
	public void paintComponent(Graphics g) {
		super.paintComponent(g);
		// Draw the Grid
		for(int i = 0; i <= GRID_WIDTH+1; i++)
			g.drawLine(0,i*GRID_DIM, 800,i*GRID_DIM);
			
		for(int j = 0; j <= GRID_HEIGHT+1; j++)
			g.drawLine(j*GRID_DIM, 0, j*GRID_DIM, 800);
		
		// Draw all connections and nodes
		long time = System.currentTimeMillis();
		for(NodeInfo node : gui.getNodes()) 
			node.drawLine(g, gateway.grid_x, gateway.grid_y);
		for(NodeInfo node : gui.getNodes()) 
			node.draw(g, time);
		
		// draw gateway
		gateway.draw(g, 0);
	}
}