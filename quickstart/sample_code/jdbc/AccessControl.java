/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
import java.io.*;
class AccessControl
{
	public String username = "appuser";
	public String getUsername() 
	{
		try
		{
			System.out.print("Enter username:");
			BufferedReader br = new BufferedReader(new InputStreamReader(System.in));
			username = br.readLine();
		} 
		catch (IOException ioe) {ioe.printStackTrace();}
		return username;
	}

	public String getPassword() 
	{
		String password = PasswordField.readPassword("Enter password for " + username +": ");
		return password;
	}

	public String getPassword( String theUsername ) 
	{
		String password = PasswordField.readPassword("Enter password for " + theUsername +": ");
		return password;
	}

	public static void main()
	{
		System.out.println("Wie gehts?");
	}

}
