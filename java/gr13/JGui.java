package gr13;
/**********************************************************
 *	18748 - Spring 2015 - Group 13 
 *	Madhav Iyengar
 *	Miguel Sotolongo
 *	Nathaniel Jeffries
 **********************************************************/

// Util
import java.util.*; 
import java.awt.EventQueue;
import java.awt.Dimension;
import java.awt.FlowLayout;
 
// GUI Components
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.JTextArea;
import javax.swing.SwingUtilities;
 
/**
 * JGui
 *  '-p string':	(Optional) Specify USB port, defaults to "/dev/ttyUSB0"
 */
public class JGui extends JFrame  implements Runnable{
	/***********************************************
	 * 	Constants							   
	 ***********************************************/
	private final int PADDING   = 5;  // GUI Padding
	private final int MAX_NODES = 10; // Max # of nodes to track
	
	/***********************************************
	 * 	Declarations							   
	 ***********************************************/
	private static String port = "/dev/ttyUSB0";
	private Map<Integer, NodeInfo> node_map;
	
	/***********************************************
	 * 	Declarations - GUI					   
	 ***********************************************/
	JPanel panel;
	JTextArea info;
	NetworkGraph graph;
	
	// Constructor
	public JGui() {
		node_map = new HashMap<Integer, NodeInfo>(MAX_NODES);
	}
	
	/// initGUI
	//  Build GUI
	public void initGUI(){
		// Layout
		panel = new JPanel();
		
		NetworkGraph graph = new NetworkGraph();
        graph.setPreferredSize(new Dimension(295, 390));

        info = new JTextArea("Are USB permissions set?");
        info.setPreferredSize(new Dimension(295, 390));
		
		panel.add(graph);
		panel.add(info);
		
		add(panel);
		pack();
		
		// Frame Properties 
		setLayout(new FlowLayout(FlowLayout.LEADING, PADDING, PADDING));
		setTitle("Group 13 - Gateway");
        setSize(600, 400);
        setLocationRelativeTo(null);
		setResizable(false);
        setDefaultCloseOperation(EXIT_ON_CLOSE);
	}
	
	/// addNode
	//  Adds or updates NodeInformation on GUI
	public void addNode(Integer mac, NodeInfo info) {
		node_map.put(mac, info);
		updateInformation();
	}
	
	/// updateInformation
	//	Refresh NodeInformation
	public void updateInformation() {
		String data = "";
		for(Map.Entry<Integer, NodeInfo> entry : node_map.entrySet()) {
			data += entry.getValue().toString();
		}
		info.setText(data);
	}
	
    //creating and showing this application's GUI.
	@Override
	public void run() {
		this.initGUI();
		this.setVisible(true);
	}
	
	/// Main
	// 	Begin serial communication
	//	Launch GUI
	public static void main(String[] args)
	{		
		/// Command Line Read
		// Interprets command line arguments and
		// updates test parameters. 
		for(int param = 0; param < args.length; ++param)
		{
			// Number of passengers 
			if(args[param].equals("-p"))
			{
				param++;
				port = args[param];
			}
		}
		
		// Init Serial Connection
		try{
			JGui gui = new JGui();
			
			// Begin Serial Read
			(new SerialComm()).connect(port, gui);
			
			//Schedule a job for the event-dispatching thread:
			EventQueue.invokeLater(gui);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}
