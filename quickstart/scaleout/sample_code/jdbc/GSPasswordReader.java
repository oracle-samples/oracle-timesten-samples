/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.io.*;

/**
 *
 * A class to securely read a password from the terminal. This is
 * done using the Java.io Console API.
 *
 */

public class GSPasswordReader 
{
    private Console cons = null;

    /**
     * Constructor
     */
    GSPasswordReader()
    {
        cons = System.console();
    } // Constructor

    public String readPassword(
                               String prompt // displayed to user
                              )
   {
       String password = null;

       if (  cons != null  )
       {
           try {
               char[] passwd = cons.readPassword("%s", prompt);
               if (  passwd != null  )
                   password = new String( passwd );
               java.util.Arrays.fill( passwd, ' ' );
           } catch ( Exception e ) { password = null; }
       }

       return password;
    }

} // GSPasswordReader
