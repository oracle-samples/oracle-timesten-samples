/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
import java.io.*;

public class PasswordField {

   /**
    *@param prompt The prompt to display to the user
    *@return The password as entered by the user
    */
   public static String readPassword (String prompt) {
      Console cons = System.console();
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

} // PasswordField
