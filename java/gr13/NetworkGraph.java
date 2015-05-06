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
	public static final int GATEWAY_X = 3;
	public static final int GATEWAY_Y = 3;
	final int GRID_WIDTH = 8; // Grid squares 
	final int GRID_HEIGHT = 8;
	final int FPS = 100;
	
	boolean grid[][];
	boolean isFirst; // Special case - 2 node compare
	int free_slots;
	JGui gui;
	Random random;
	NodeInfo gateway;
	
	public NetworkGraph(JGui gui) { 
		this.gui = gui; 
		this.grid = new boolean[GRID_WIDTH][GRID_HEIGHT];
		this.random = new Random();
		this.isFirst = true;
		
		// Manually add gateway node
		gateway = new NodeInfo(0, "");
		gateway.grid_x = GATEWAY_X;
		gateway.grid_y = GATEWAY_Y;
		grid[GATEWAY_X][GATEWAY_Y] = true; // Reserved for gateway node
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
			int r_row = (isFirst) ? 3 : random.nextInt(GRID_WIDTH);
			int r_col = (isFirst) ? 0 : random.nextInt(GRID_HEIGHT);
			if(!grid[r_row][r_col]){
				// Populate grid and all diagonals
				grid[r_row][r_col] = true;
				fillDiag(r_row, r_col);
				isPlaced = true;
				node.grid_x = r_row;
				node.grid_y = r_col;
				free_slots--;
				isFirst = false;
			}	
		}
	}
	private void fillDiag(int x, int y) {
		// HACK: Fill entire diagonal away and towards gateway
		// 		 this is done to prevent node path from overlapping
		//       and giving the illusion of a hop
		if(x == 3 && y < 3) 
			for(int i = 0; i < 3; i++) grid[x][i] = true;
		else if(x == 3 && y > 3)
			for(int i = 0; i <= 3; i++) grid[x][4 + i] = true;
		else if(y == 3 && x < 3)
			for(int i = 0; i < 3; i++) grid[i][y] = true;
		else if(y == 3 && x > 3)
			for(int i = 0; i <= 3; i++) grid [4+i][y] = true;
		else if(x == y && x < 3)
			for(int i = 0; i < 3; i++) grid[i][i] = true;
		else if(x == y && x > 3)
			for(int i = 0; i <= 3; i++) grid[4+i][4+i] = true;
		else if(x + y == 6 && x < 3)
			for(int i = 0; i < 3; i++) grid[i][6 - i] = true;
		else if(x + y == 6 && x > 3)
			for(int i = 0; i < 3; i++) grid[6-i][i] = true;
	}
	
	/// Draw Methods
	public void paintComponent(Graphics g) {
		super.paintComponent(g);
		
		// Nodes
		NodeInfo[] nodes = gui.getNodes();
		long time = System.currentTimeMillis();
		if(nodes.length == 1)
		{
			// Single Node
			// Further visualize node drift
			nodes[0].drawDrift(g);
			nodes[0].drawLine(g, gateway.grid_x, gateway.grid_y);
			nodes[0].draw(g, time);
		} else {
			// Draw the Grid
			for(int i = 0; i <= GRID_WIDTH+1; i++)
				g.drawLine(0,i*GRID_DIM, 800,i*GRID_DIM);
			
			for(int j = 0; j <= GRID_HEIGHT+1; j++)
				g.drawLine(j*GRID_DIM, 0, j*GRID_DIM, 800);
			
			// Draw all connections and nodes
			for(NodeInfo node : nodes) 
				node.drawLine(g, gateway.grid_x, gateway.grid_y);
			for(NodeInfo node : nodes) 
				node.draw(g, time);
			for(NodeInfo node : nodes) 
				node.drawText(g);
		}
		
		// always draw gateway
		gateway.draw(g, 0);
	}
}