/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
import java.io.PrintStream;

public class IOLibrary
{

	boolean opt_doClient;
	boolean opt_doTrace;
	String opt_connstr;
	PrintStream errStream;

	public IOLibrary(PrintStream stream)
	{
		errStream = stream;
		opt_connstr = "";
	}

	/**
	 * Parse Options from the command line
	 *
	 * @param args  String array of arguments to parse
	 * @param usage Print this usage String on errors during argument parsing
	 *
	 * @return true on success, false otherwise
	 */
	public String getUsageString(String programName) 
	{
		String usageString = "\n" +
			"Usage: "+ programName +" [-t] [-d|-c] connstr\n" +
			"       "+ programName +" -h | -help \n" +
			"       "+ programName +" -V \n" +
			"  -h | -help    print usage and exit\n" +
			"  -V            print the TimesTen version\n"+
			"  -d            connect using direct driver (default)\n" +
			"  -c            connect using client driver\n" +
			"  -t            enable JDBC tracing\n" +
			"  connstr      connection string, e.g. \"DSN=" +
                           tt_version.sample_DSN + "\"\n"+
			"  Usage examples:\n"+
			"	  java "+programName+"\n"+
			"	  java "+programName+" -c\n"+ 
			"	  java "+programName+" mydsn_name\n"+
			"Program defaults if parameters not specified:\n"+
			"connstr=\"DSN=" + tt_version.sample_DSN +
                        "\", -d for direct-linked";

		return usageString;
	}

	public String getUsageStringDummy(String programName)
	{
		String message = "Use " + programName + "appropriately\n";
		return message;
	}
	

	public boolean parseOpts(String[] args, String usage)
	{
		// Parse Command line
		int i;
		boolean argsOk;
		String arg;
		for (i = 0, argsOk = true, arg = null;
			argsOk && (i < args.length) && ((arg = args[i]).startsWith("-"));
			i++) 
		{

			if (arg.equals("-h") || arg.equals("-help")) 
			{
				argsOk = false;
			} 
			else if (arg.equals("-t")) 
			{
				opt_doTrace = true;
			} 
			else if (arg.equals("-c")) 
			{
				opt_doClient = true;

			} 
			else if (arg.equals("-d"))
			{
				opt_doClient = false;
			} 
			else if (arg.equals("-V"))
			{
				//opt_PrintVersion = true;
				System.out.println(tt_version.TTVERSION_STRING);
				System.exit(0);
			} 
			else 
			{
				errStream.println("Illegal option '" + arg + "'");
				argsOk = false;
			}

		} // end of for() loop for argument parsing

		// Check if argument parsing exited for error reasons
		if (!argsOk) 
		{
			errStream.println(usage);
			return argsOk;
		}

		// Set the connection string
		if (i < args.length) 
		{
			opt_connstr = arg;
			i++;
		}

		// Check for unparsed arguments
		if (i != args.length) 
		{
			errStream.println("Extra arguments left on command line");
			errStream.println(usage);
			return false;
		}

		// Check for null or unset connection string
		if (opt_connstr.equals("")) 
		{
			if (opt_doClient) 
			    opt_connstr = tt_version.sample_DSN_CS;
			else 
			    opt_connstr = tt_version.sample_DSN;
		}
		return true;
	}

	public String getName() 
    {
		Class c = this.getClass();
		String name = c.getName();
		return name;
    }

	//This Tests the class functionality
	public static void main(String args[])
	{
		PrintStream errStream = System.err;
		IOLibrary myLib = new IOLibrary(errStream);
		String myName = myLib.getName();
		System.out.println("Class Name is:"+myName);
		String usageStringOne = myLib.getUsageStringDummy(myName);
		String usageStringTwo= myLib.getUsageString("level1");
		String correctargs[] = {"-c",tt_version.sample_DSN};
		String incorrectargs[] = {"-inappropriate"};
		boolean testOnePassed = false;
		boolean testTwoPassed = false;

		//Test One
		if (myLib.parseOpts(correctargs, usageStringTwo)) 
		{
		  System.out.println(correctargs[0]+" "+ correctargs[1]+ " is correct");
		  testOnePassed = true;
		} else 
		{
		  System.out.println(correctargs[0]+" "+ correctargs[1]+ " is incorrect"); 
		}

		if (testOnePassed) 
		{
			System.out.println("*** Test One passed ***");
		} else
		{
			System.out.println("*** Test One failed ***");
		}
 
		//Test Two
		if (myLib.parseOpts(incorrectargs, usageStringTwo)) 
		{
		  System.out.println(incorrectargs[0] + " is correct");
		} else 
		{
		  System.out.println(incorrectargs[0] + " is incorrect");
		  testTwoPassed = true;
		}

		if (testTwoPassed) 
		{
			System.out.println("*** Test Two passed ***");
		} else
		{
			System.out.println("*** Test Two failed ***");
		}


	}

}
