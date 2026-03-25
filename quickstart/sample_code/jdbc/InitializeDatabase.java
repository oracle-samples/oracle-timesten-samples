/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
import java.sql.*;

public class InitializeDatabase
{
	public static String[] command;
	public static int num_commands;

	/**
	 * Name of TimesTen driver
	 */
	private static final String timestenDriver = "com.timesten.jdbc.TimesTenDriver";

	/**
	 * Prefix URL to pass to DriverManager.getConnection() for
	 * TimesTen Direct Connection
	 */
	private static final String directURLPrefix = "jdbc:timesten:direct:";





	public InitializeDatabase() 
	{
		command = new String[100];

		command[1] = "truncate table order_item;";
		command[2] = "truncate table orders;";
		command[3] = "truncate table inventory;";
		command[4] = "truncate table product;";
		command[5] = "truncate table customer;";


		command[6] = "insert into product values(100, 'Beef Flavor', 1.10, 0.25, 'Tasty artificial beef flavor crystals',NULL, 'Not for use in soft drinks');";
		command[7] = "insert into product values(101, 'Chicken Flavor', 0.95, 0.25, 'Makes everything taste like chicken',NULL, 'Not for use in soft drinks');";
		command[8] = "insert into product values(102, 'Garlic extract', 0.10, 0.22, 'Pure essence of garlic',NULL, 'Keeps vampires away');";
		command[9] = "insert into product values(103, 'Oat bran', 4.00, 5.055, 'All natural oat bran',NULL, 'May reduce cholesterol');";

		command[10] = "insert into customer values(1, 'South', 'Fiberifics', '123 any street');";
		command[11] = "insert into customer values(2, 'West', 'Natural Foods Co.', '5150 Johnson Rd');";
		command[12] = "insert into customer values(3, 'North', 'Happy Food Inc.', '4004 Happy Lane');";
		command[13] = "insert into customer values(4, 'East', 'Nakamise', '6-6-2 Nishi Shinjuku');";

		command[14] = "insert into orders values(1001, 1, sysdate, null, null);";
		command[15] = "insert into order_item values(1001, 103, 50);";


		command[16] = "insert into orders values(1002, 2, '2003-04-11 09:17:32.148000', null, null);";
		command[17] = "insert into order_item values(1002, 102, 6000);";
		command[18] = "insert into order_item values(1002, 103, 500);";


		command[19] = "insert into orders values(1003, 3, '2003-03-09 08:15:12.100000', null, null);";
		command[20] = "insert into order_item values(1003, 101, 40);";
		command[21] = "insert into order_item values(1003, 102, 500);";

		command[22] = "insert into orders values(1008, 3, sysdate, null, null);";
		command[23]	=	"insert into order_item values(1008, 102, 300);";

		command[24] = "insert into orders values(1009, 4, sysdate, null, null);";
		command[25] = "insert into order_item values(1009, 103, 79);";


		command[26] = "insert into inventory values(100, 'London', 10000);";
		command[27] = "insert into inventory values(101, 'New York', 5000);";
		command[28] = "insert into inventory values(102, 'Gilroy', 1000);";
		command[29] = "insert into inventory values(103, 'Gilroy', 4000);";

		command[30] = "insert into customer values(10, 'East', 'Some name', 'Some Address');";
		command[31] = "update customer set name = 'Another Name', address = 'Another address' where cust_num = 10;";
		command[32] = "delete from customer where cust_num = 10;";

		num_commands = 32;
	}

	public void initialize(Connection con) 
	{
		Statement stmt=null;
		try {
		  stmt = con.createStatement();
		} catch (SQLException sqle) {sqle.printStackTrace();}
		for (int i = 1; i <= num_commands; i++) 
		{
                  try {
                    //System.out.println("["+i+"]"+command[i]);
                    stmt.executeUpdate(command[i]);
                  }
                  catch (SQLException sqle)  
                  {   
                    sqle.printStackTrace();
                  }
				
                }
	} 
		


	public static void main(String[] args) {
	try{
		Connection con;
		Class.forName(timestenDriver);
		AccessControl ac = new AccessControl();
		String username = ac.getUsername();
		String password = ac.getPassword();
		String connStr ="sampledb";
		String URL = directURLPrefix + connStr;
		con = DriverManager.getConnection(URL, username, password);
		InitializeDatabase idb = new InitializeDatabase();
		idb.initialize(con);
	} catch (Exception e) {e.printStackTrace();}

	}


}
