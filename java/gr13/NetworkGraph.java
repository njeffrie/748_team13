package gr13;
/**********************************************************
 *	18748 - Spring 2015 - Group 13 
 *	Madhav Iyengar
 *	Miguel Sotolongo
 *	Nathaniel Jeffries
 **********************************************************/

import java.awt.*;
import javax.swing.JPanel;


public class NetworkGraph extends JPanel {
	/// Graph Parameters
	final int oval_width = 50;
	final int oval_height = 50;
	final int gateX = 125;
	final int gateY = 125;
	
	public NetworkGraph() {}
	
	public void paintComponent(Graphics g) {
		// test render - nodes
		drawOval(g, 50, 50, "1", gateX, gateY);
		drawOval(g, 50, 125, "2", gateX, gateY);
		drawOval(g, 200, 125, "3", gateX, gateY);
		drawOval(g, 125, 50, "4", gateX, gateY);
		
		// test render - gate
		drawOval(g, 125, 125, "G");
	}
	
	private void drawOval(Graphics g, int centerX, 
		int centerY, String text, int gateX, int gateY) 
	{
		// Set color
		g.setColor(Color.black);
		
		// Line to gateway
		g.drawLine(centerX, centerY, gateX, gateY);
		
		// Draw Oval
		g.fillOval(
			centerX-oval_width/2, centerY-oval_height/2, 
			oval_width, oval_height
		);
		
		g.setColor(Color.white);
		// Draw text
        FontMetrics fm = g.getFontMetrics();
        double textWidth = fm.getStringBounds(text, g).getWidth();
        g.drawString(text, (int) (centerX - textWidth/2),
                    (int) (centerY + fm.getMaxAscent() / 2));
	}
	
	private void drawOval(Graphics g, int centerX, int centerY, String text) {
		// Set color
		g.setColor(Color.black);
		
		// Draw Oval
		g.fillOval(
			centerX-oval_width/2, centerY-oval_height/2, 
			oval_width, oval_height
		);
		
		g.setColor(Color.white);
		// Draw text
        FontMetrics fm = g.getFontMetrics();
        double textWidth = fm.getStringBounds(text, g).getWidth();
        g.drawString(text, (int) (centerX - textWidth/2),
                    (int) (centerY + fm.getMaxAscent() / 2));
	}

}
