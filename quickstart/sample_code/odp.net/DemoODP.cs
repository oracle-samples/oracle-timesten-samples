using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using Oracle.DataAccess.Client;
using Oracle.DataAccess.Types;
using System.Data;
using System.Data.Common;

/*
 * This is TimesTen ODP.NET demo program.
 * It illustrates the following functionality:
 * 1. Executing plain SQL statement
 * 2. Executing PLSQL block, using IN parameters
 * 3. Executing SQL statement using IN parameters
 * 4. Array binding (batch insert)
 * 5. Executing database procedure using IN and OUT parameters
 * 6. Using REF CURSOR
 * 
 * There are few database object required for this demo:
 * 1. Table DEPT
 *          CREATE TABLE dept(
                deptno     number(2) not null primary key,
                dname      varchar2(14),
                loc        varchar2(13));
            INSERT INTO dept VALUES (10,'ACCOUNTING','NEW YORK');
            INSERT INTO dept VALUES (20,'RESEARCH','DALLAS');
            INSERT INTO dept VALUES (30,'SALES','CHICAGO');
            INSERT INTO dept VALUES (40,'OPERATIONS','BOSTON');

 * 2. Table EMP
 *          CREATE TABLE emp(
                empno      number(4) not null primary key,
                ename      varchar2(10),
                job        varchar2(9),
                mgr        number(4),
                hiredate   date,
                sal        number(7,2),
                comm       number(7,2),
                deptno     number(2),
                foreign key (deptno) references dept(deptno));
            INSERT INTO emp VALUES (7369,'SMITH','CLERK',7902,to_date('17-12-1980','dd-mm-yyyy'),800,NULL,20);
            INSERT INTO emp VALUES (7499,'ALLEN','SALESMAN',7698,to_date('20-2-1981','dd-mm-yyyy'),1600,300,30); 
            INSERT INTO emp VALUES (7521,'WARD','SALESMAN',7698,to_date('22-2-1981','dd-mm-yyyy'),1250,500,30);
            INSERT INTO emp VALUES (7566,'JONES','MANAGER',7839,to_date('2-4-1981','dd-mm-yyyy'),2975,NULL,20);
            INSERT INTO emp VALUES (7654,'MARTIN','SALESMAN',7698,to_date('28-9-1981','dd-mm-yyyy'),1250,1400,30);
            INSERT INTO emp VALUES (7698,'BLAKE','MANAGER',7839,to_date('1-5-1981','dd-mm-yyyy'),2850,NULL,30);
            INSERT INTO emp VALUES (7782,'CLARK','MANAGER',7839,to_date('9-6-1981','dd-mm-yyyy'),2450,NULL,10);
            INSERT INTO emp VALUES (7788,'SCOTT','ANALYST',7566,to_date('13-JUL-1987','dd-mon-yyyy')-numtodsinterval(85,'day'),3000,NULL,20);
            INSERT INTO emp VALUES (7839,'KING','PRESIDENT',NULL,to_date('17-11-1981','dd-mm-yyyy'),5000,NULL,10);
            INSERT INTO emp VALUES (7844,'TURNER','SALESMAN',7698,to_date('8-9-1981','dd-mm-yyyy'),1500,0,30);
            INSERT INTO emp VALUES (7876,'ADAMS','CLERK',7788,to_date('13-JUL-1987','dd-mon-yyyy')-numtodsinterval(51,'day'),1100,NULL,20);
            INSERT INTO emp VALUES (7900,'JAMES','CLERK',7698,to_date('3-12-1981','dd-mm-yyyy'),950,NULL,30;
            INSERT INTO emp VALUES (7902,'FORD','ANALYST',7566,to_date('3-12-1981','dd-mm-yyyy'),3000,NULL,20;
            INSERT INTO emp VALUES (7934,'MILLER','CLERK',7782,to_date('23-1-1982','dd-mm-yyyy'),1300,NULL,10);

 * 3. Package emp_pkg
 * 
 * The above objects are created and tables are populated when executing build_sampledb.bat script
 * located within quickstart in TimesTen installation.
 */

public class DemoODP
{
   public static string TEST_NAME = "DemoODP";

   private static string connStr = "";
   private static string db = "";
   private static string user = "";
   private static string pwd = "";
   private static bool dbg = false;
   private static string logFile = "";
   private StreamWriter writer = null;
   private int empPK = 0;
   private int deptPK = 0;
   private int mgrID = 0;
   private OracleConnection conn = null;

   public DemoODP()
   {
      try
      {
         if (!logFile.Equals(""))
         {
            OpenLogFile();
         }

         PrintLine("Start Test");
         if (dbg) PrintLine("connStr = " + connStr);
         conn = GetConnection(connStr);

         // Determine the highest employee number
         // will be used to keep employee number unique
         empPK = GetMaxPK("EMP", "EMPNO");
         if (dbg) PrintLine("empPK = " + empPK);

         // Determine the highest deprtment number 
         // will be used to keep department number unique
         deptPK = GetMaxPK("DEPT", "DEPTNO");
         if (dbg) PrintLine("deptPK = " + deptPK);

         // Adding new deparment using PLSQL block and IN parameters
         deptPK = AddNewDepartment(deptPK, "IT", "HOUSTON");
         if (dbg) PrintLine("New department added: deptPK = " + deptPK);

         // Adding manager to the newly created department using PLSQL block and IN parameters
         empPK = AddNewManager(empPK, "ITMGR", deptPK);
         mgrID = empPK;
         if (dbg) PrintLine("New manager added: mgrID = " + mgrID);

         int numOfEmps = 10;
         int empBasicSalary = 2000;

         // Adding numOfEmps employees to the new department using array binding
         empPK = AddEmployees(numOfEmps, empPK, deptPK, mgrID, empBasicSalary);
         if (dbg) PrintLine("New employees added to depatment " + deptPK);

         // Use a PLSQL package procedure to randomly give one of the lowest paid workers a payraise.
         GivePayRaise();
         if (dbg) PrintLine("Pay rase was granted to one employee");

         // Use REF CURSOR to display employees from particular department
         ShowDepatmentEmps(deptPK);

         conn.Close();
         conn.Dispose();

         PrintLine("Test finished");
         if (!logFile.Equals(""))
         {
            CloseLogFile();
         }
      }
      catch (Exception e)
      {
         Console.WriteLine("Test Failed");
         Console.WriteLine(e.Message);
         Console.WriteLine(e.StackTrace);
         Environment.Exit(-1);
      }
   }

   private OracleConnection GetConnection(String connStr)
   {
      OracleConnection con = new OracleConnection();
      con.ConnectionString = connStr;

      con.Open();
      return con;
   }

   /*
    * Using plain SQL with OracleDataReader 
    * to get the highest value in a table primary key field 
    */
   private int GetMaxPK(string tableName, string pkFiledName) 
   {
      int maxPK = 0;
      string qryStr = "select max(" + pkFiledName + ") maxno " +
                      "from " + tableName;
      OracleCommand select = null;
      OracleDataReader reader = null;
      try
      {
         select = new OracleCommand(qryStr, conn);
         select.CommandType = CommandType.Text;
         reader = select.ExecuteReader();
         if (reader.Read())
         {
            object[] values = new object[1];
            reader.GetValues(values);
            if (values[0] != DBNull.Value)
            {
               maxPK = Convert.ToInt32(values[0]);
            } 
         }
      }
      finally
      {
         reader.Close();
         select.Dispose();
      }

      return maxPK;
   }

   /* 
    * Using PLSQL block with IN parameters to add new department
    */ 
   private int AddNewDepartment(int deptPK, string deptName, string deptLocation)
   {
      int deptNo = deptPK + 1;
      int tableColumns = 3;
      string insertStmt =
         "declare " +
         "  v_deptno DEPT.DEPTNO%TYPE; " +
         "  v_dname  DEPT.DNAME%TYPE; " +
         "  v_loc    DEPT.LOC%TYPE; " +
         "begin " +
         "  v_deptno := :1; " +
         "  v_dname := :2; " +
         "  v_loc := :3; " +
         "  insert into DEPT values(v_deptno, v_dname, v_loc); " +
         "end;";
      OracleCommand insert = null;
      OracleParameter[] inParArr = new OracleParameter[tableColumns];
      try
      {
         insert = new OracleCommand(insertStmt, conn);
         insert.CommandType = CommandType.Text;

         // DEPTNO
         inParArr[0] = insert.CreateParameter();
         inParArr[0].OracleDbType = OracleDbType.Int32;
         inParArr[0].Value = deptNo;
         inParArr[0].Direction = ParameterDirection.Input;

         // DNAME
         inParArr[1] = insert.CreateParameter();
         inParArr[1].OracleDbType = OracleDbType.Varchar2;
         inParArr[1].Value = deptName;
         inParArr[1].Direction = ParameterDirection.Input;

         // LOC
         inParArr[2] = insert.CreateParameter();
         inParArr[2].OracleDbType = OracleDbType.Varchar2;
         inParArr[2].Value = deptLocation;
         inParArr[2].Direction = ParameterDirection.Input;

         if (dbg) PrintLine("AddNewDepartment --> inserting values = " + 
            deptNo + "; " + deptName + "; " + deptLocation);

         insert.Parameters.AddRange(inParArr);
         insert.ExecuteNonQuery();
      }
      finally
      {
         insert.Dispose();
      }
      return deptNo;
   }

   /* 
    * Using PLSQL blocks with IN/OUT parameters
    * 
    * adding new employee that reports to the PRESIDENT
    * has salary that is half of PRESIDENT's salary
    * and has title 'MANAGER'
    */ 
   private int AddNewManager(int empPK, string empName, int deptNo)
   {
      int mgrNo = empPK + 10;
      int presidentId = 0;
      double presidentSal = 0;

      // Getting PRESIDENT's employee number and salary
      // PRESIDENT does not report to anyone, thus MGR fieled is NULL
      string selectStmt = 
         "begin " +
         "  select empno, sal " +
         "  into :1, :2 " +
         "  from emp " +
         "  where mgr is null " +
         "  and rownum < 2; " +     // just in case that for some reason there are more than one PRESIDENT
         "end;";
      OracleCommand select = null;
      try
      {
         select = new OracleCommand(selectStmt, conn);
         select.CommandType = CommandType.Text;

         OracleParameter[] outParArr = new OracleParameter[2];

         // EMPNO
         outParArr[0] = select.CreateParameter();
         outParArr[0].OracleDbType = OracleDbType.Int32;
         outParArr[0].Direction = ParameterDirection.Output;

         // SALARY
         outParArr[1] = select.CreateParameter();
         outParArr[1].OracleDbType = OracleDbType.Double;
         outParArr[1].Direction = ParameterDirection.Output;

         select.Parameters.AddRange(outParArr);
         select.ExecuteNonQuery();
         
         presidentId = Convert.ToInt32(outParArr[0].Value.ToString());
         presidentSal = Convert.ToDouble(outParArr[1].Value.ToString());
         
      }
      finally
      {
         select.Dispose();
      }

      // Adding new MANAGER to new depatment
      string insertStmt = "insert into emp (empno, ename, job, mgr, hiredate, sal, comm, deptno) " +
         "values (:1, :2, 'MANAGER', :3, sysdate, :4, null, :5)";
      double salary = Math.Round(presidentSal/2);

      OracleCommand insert = null;
      try
      {
         insert = new OracleCommand(insertStmt, conn);
         insert.CommandType = CommandType.Text;

         OracleParameter[] inParArr = new OracleParameter[5];

         // EMPNO
         inParArr[0] = insert.CreateParameter();
         inParArr[0].OracleDbType = OracleDbType.Int32;
         inParArr[0].Value = mgrNo;
         inParArr[0].Direction = ParameterDirection.Input;

         // ENAME
         inParArr[1] = insert.CreateParameter();
         inParArr[1].OracleDbType = OracleDbType.Varchar2;
         inParArr[1].Value = empName;
         inParArr[1].Direction = ParameterDirection.Input;

         // MGR
         inParArr[2] = insert.CreateParameter();
         inParArr[2].OracleDbType = OracleDbType.Int32;
         inParArr[2].Value = presidentId;
         inParArr[2].Direction = ParameterDirection.Input;

         // SAL
         inParArr[3] = insert.CreateParameter();
         inParArr[3].OracleDbType = OracleDbType.Double;
         inParArr[3].Value = salary;
         inParArr[3].Direction = ParameterDirection.Input;

         // DEPTNO
         inParArr[4] = insert.CreateParameter();
         inParArr[4].OracleDbType = OracleDbType.Int32;
         inParArr[4].Value = deptNo;
         inParArr[4].Direction = ParameterDirection.Input;

         if (dbg) PrintLine("AddNewManager --> inserting values = " +
            mgrNo + ", " + empName + ", " + presidentId + ", " + salary + ", " + deptNo);

         insert.Parameters.AddRange(inParArr);
         insert.ExecuteNonQuery();
      }
      finally
      {
         insert.Dispose();
      }
      return mgrNo;
   }

   /*
    * Using array binding (batch) to insert number of new employees
    */
   private int AddEmployees(int numOfEmps,
                            int empPK,
                            int deptPK, 
                            int mgrID, 
                            double empBasicSalary)
   {
      // Creating arrays to contain data relevant to each field in the table 
      int[] empnoArr = new int[numOfEmps];
      string[] enameArr = new string[numOfEmps];
      string[] jobArr = new string[numOfEmps];
      int[] mgrArr = new int[numOfEmps];
      DateTime[] hiredateArr = new DateTime[numOfEmps];
      double[] salArr = new double[numOfEmps];
      object[] commArr = new object[numOfEmps];
      int[] deptnoArr = new int[numOfEmps];
      DateTime hireDate = DateTime.Today;

      string insertStmt =
         "insert into emp (empno, ename, job, mgr, hiredate, sal, comm, deptno) " +
         "values (:1, :2, :3, :4, :5, :6, :7, :8)";

      // Populating arrays with data
      for (int emp = 0; emp < numOfEmps; emp++)
      {
         empnoArr[emp] = ++empPK;
         enameArr[emp] = "DVLPR" + (emp + 1);
         jobArr[emp] = "DEVELOPER";
         mgrArr[emp] = mgrID;
         hiredateArr[emp] = hireDate;
         salArr[emp] = empBasicSalary;
         commArr[emp] = DBNull.Value;
         deptnoArr[emp] = deptPK;
         if (dbg) PrintLine(empnoArr[emp] + ", " +
                   enameArr[emp] + ", " +
                   jobArr[emp] + ", " +
                   mgrArr[emp] + ", " +
                   hiredateArr[emp] + ", " +
                   salArr[emp] + ", " +
                   commArr[emp] + ", " +
                   deptnoArr[emp]);
      }

      int param = 0;

      OracleCommand insert = null;
      try
      {
         insert = new OracleCommand(insertStmt, conn);
         insert.CommandType = CommandType.Text;
         insert.ArrayBindCount = numOfEmps;

         OracleParameter[] inParArr = new OracleParameter[8];  // 8 is number of columns in EMP table

         // Setting parameters for every column in EMP table
         //
         // EMPNO
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.Int32;
         inParArr[param].Value = empnoArr;
         inParArr[param++].Direction = ParameterDirection.Input;

         // ENAME
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.Varchar2;
         inParArr[param].Value = enameArr;
         inParArr[param++].Direction = ParameterDirection.Input;

         // JOB
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.Varchar2;
         inParArr[param].Value = jobArr;
         inParArr[param++].Direction = ParameterDirection.Input;

         // MGR
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.Int32;
         inParArr[param].Value = mgrArr;
         inParArr[param++].Direction = ParameterDirection.Input;
         
         // HIREDATE
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.TimeStamp;
         inParArr[param].Value = hiredateArr;
         inParArr[param++].Direction = ParameterDirection.Input;

         // SAL
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.Double;
         inParArr[param].Value = salArr;
         inParArr[param++].Direction = ParameterDirection.Input;

         // COMM
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.Varchar2;
         inParArr[param].Value = commArr;
         inParArr[param++].Direction = ParameterDirection.Input;

         // DEPTNO
         inParArr[param] = insert.CreateParameter();
         inParArr[param].OracleDbType = OracleDbType.Int32;
         inParArr[param].Value = deptnoArr;
         inParArr[param++].Direction = ParameterDirection.Input;

         insert.Parameters.AddRange(inParArr);
         insert.ExecuteNonQuery();
      }
      finally
      {
         insert.Dispose();
      }
      // returning the highest employee number
      return empnoArr[numOfEmps - 1];
   }

   /*
    * Using database procedure emp_pkg.givePayRaise to give a payrase to random employee
    * 
    * The procedure excepts the number of employees 
    * that will participate in random picking up one lucky employee,
    * which gets a 10% pay raise
    * In case that something goes wrong in the procedure
    * the error number and message are returned to the calling code.
    */
   private void GivePayRaise()
   {
      int numEmps = 10;
      int enameSize = 10;
      int errMsgSize = 200; // the message in truncated to 200 characters (see emp_pkg.givePayRaise procedure)
      //int errCode = 0;
      //string errMsg = "";

      string updateStmt = "begin emp_pkg.givePayRaise(:numEmps, :empName, :errCode, :errText); end;";

      OracleCommand update = null;
      try
      {
         update = new OracleCommand(updateStmt, conn);
         //OracleParameter[] parArr = new OracleParameter[4];
         // first is IN parameter - passing number of employees to choose from
         OracleParameter param1 = update.Parameters.Add("numEmps", 
            OracleDbType.Int32, numEmps, ParameterDirection.Input);

         OracleParameter param2 = update.Parameters.Add("empName",
            OracleDbType.Varchar2, enameSize, DBNull.Value, ParameterDirection.Output);

         OracleParameter param3 = update.Parameters.Add("errCode", 
            OracleDbType.Int32, DBNull.Value, ParameterDirection.Output);

         OracleParameter param4 = update.Parameters.Add("errText",
            OracleDbType.Varchar2, errMsgSize, DBNull.Value, ParameterDirection.Output);

         //update.Parameters.AddRange(parArr);
         update.ExecuteNonQuery();

         if (Convert.ToInt32(param3.Value.ToString()) == 0) 
         {
            PrintLine("The employee who got the 10% pay raise was " + param2.Value);
         }
         else
         {
            throw new Exception("A problem with emp_pkg.givePayRaise\n" +
               "  Error: " + param3.Value + "\n" +
               "  Error Text: " + param4.Value);
         }
      }
      finally
      {
         update.Dispose();
      }

   }

   /*
    * Using REF CURSOR to select employees from certain department
    * 
    * REF CURSOR returned using OUT parameter as RefCursor object,
    * and then DataTable object is used to read data from RefCursor.
    */
   private void ShowDepatmentEmps(int deptNo)
   {
      string selectStmt = 
         "begin " +
         "  open :emp_cur for " +
         "  select empno, ename, job, mgr, hiredate, sal, comm, deptno " +
         "  from emp " +
         "  where deptno = :dept_no; " +
         "end;";

      OracleCommand select = null;

      try
      {
         select = new OracleCommand(selectStmt, conn);
         select.CommandType = CommandType.Text;

         // RefCursor is OUT parameter
         OracleParameter outRefPrm = select.Parameters.Add("emp_cur", 
            OracleDbType.RefCursor, DBNull.Value, ParameterDirection.Output);

         // PLSQL block accepts deaprtment number as IN parameter
         OracleParameter inDeptNo = select.Parameters.Add("dept_no", 
            OracleDbType.Int32, deptNo, ParameterDirection.Input);
         select.ExecuteNonQuery();

         OracleDataAdapter adapter = new OracleDataAdapter(select);
         DataTable empsDataTable = new DataTable();
         adapter.Fill(empsDataTable);

         string empDetails = "";
         PrintLine("\nEmployees in department #" + deptNo + ":");
         foreach (System.Data.DataRow row in empsDataTable.Rows)
         {
            empDetails = "";
            foreach (System.Data.DataColumn column in empsDataTable.Columns)
            {
               if (!row[column].Equals(DBNull.Value))
               {
                  empDetails = empDetails + row[column] + ", ";
               }
               else
               {
                  empDetails = empDetails + "<NULL>, ";
               }
            }
            PrintLine(empDetails.Substring(0, (empDetails.Length - 2)));
         }
      }
      finally
      {
         select.Dispose();
      }
   }

   /*
    * To execute this demo program the following arguments are required:
    *    db - the datastore name
    *    user - database user connecting to the datastore
    *    passwd - database user password
    *    
    * The following arguments are not required:
    *    log - the name of log file. If not provided then all messages will be printed into standard output
    *    verbose - if prvided then more messages will be send into output
    *    help - prints out the usage message
    */
   public static void Main(string[] args)
   {
      ParseArgs(args);
      new DemoODP();
   }

   private static void ParseArgs(string[] args)
   {
      string[] argsList = { "db", "user", "passwd", "log", "verbose", "help" };
      int idxInList;
      string errMsg = "";

      for (int idx = 0; idx < args.Length && args[idx].StartsWith("-"); idx++)
      {
         string currArg = args[idx].ToLower().Substring(1);
         idxInList = -1;
         for (int i = 0; i < argsList.Length; i++)
         {
            if (currArg.Equals(argsList[i]))
            {
               if (idxInList < 0)
               {
                  idxInList = i;
               }
               else
               {
                  Console.WriteLine("Argument " + currArg + " is ambiguous");
                  Environment.Exit(1);
               }
            }
         }

         switch (idxInList)
         {
            case 0: // db
               try
               {
                  db = args[++idx];
               }
               catch (Exception)
               {
                  errMsg = errMsg + "\nInvalid db argument";
               }
               break;

            case 1: // user
               try
               {
                  user = args[++idx];
               }
               catch (Exception)
               {
                  errMsg = errMsg + "\nInvalid user argument";
               }
               break;

            case 2: // passwd
               try
               {
                  pwd = args[++idx];
               }
               catch (Exception)
               {
                  errMsg = errMsg + "\nInvalid user argument";
               }
               break;

            case 3: // log file
               try
               {
                  logFile = args[++idx];
               }
               catch (Exception)
               {
                  errMsg = errMsg + "\nInvalid log file argument";
               }
               break;

            case 4: // verbose
               dbg = true;
               break;

            case 5: // help
               PrintUsage();
               break;
         }
      }
      if (!errMsg.Equals(""))
      {
         Console.WriteLine(errMsg);
         PrintUsage();
      }
      if (db.Equals("") || user.Equals("") || pwd.Equals(""))
      {
         Console.WriteLine("db/user/passwd are required arguments");
         PrintUsage();
      }
      connStr = "Data Source=" + db + ";user id=" + user + ";password=" + pwd + ";Statement Cache Size=0";
   }

   private static void PrintUsage()
   {
      Console.WriteLine("Usage: DemoODP -db <database_name> -user <user_name> -passwd <user_password> [-log <file_name>] [-verbose] [-help]");
      Console.WriteLine("\n\t-db\t\tspecifies the *required* datastore name");
      Console.WriteLine("\n\t-user\t\tspecifies the *required* datastore user name");
      Console.WriteLine("\n\t-passwd\t\tspecifies the *required* user password");
      Console.WriteLine("\n\t-log\t\tinstructs to print out debug messages into file instead of standard output (not required)");
      Console.WriteLine("\n\t-verbose\tinstructs to print out debug messages (not required)");
      Console.WriteLine("\n\t-help\t\tprints this usage message");
      Environment.Exit(1);
   }

   public void OpenLogFile()
   {
      try
      {
         writer = new StreamWriter(logFile, false);
         writer.AutoFlush = true;
      }
      catch (Exception e)
      {
         Console.WriteLine("Error in DemoODP.OpenLogFile()");
         Console.WriteLine(e);
         Environment.Exit(1);
      }
   }

   public void PrintLine(string LogMsg)
   {
      if (logFile.Equals(""))
      {
         Console.WriteLine(LogMsg);
      }
      else
      {
         try
         {
            writer.WriteLine(LogMsg);
         }
         catch (Exception e)
         {
            Console.WriteLine("Error in DemoODP.PrintLine()");
            Console.WriteLine(e);
            Environment.Exit(1);
         }
      }
   }

   public void CloseLogFile()
   {
      if (writer != null)
      {
         try
         {
            writer.Close();
         }
         catch (Exception e)
         {
            Console.WriteLine("Error in DemoODP.CloseLogFile()");
            Console.WriteLine(e);
            Environment.Exit(1);
         }
      }
   }
}

