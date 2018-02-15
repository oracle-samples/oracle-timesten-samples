/*
 * FileName  : qsfuncs.js
 * FileType  : Javascript
 * Author    : Steven Camina
 * Created   : June 2011
 * Version   : v1
 *
 * JavaScript functions necessary for the TimesTen Quick Start Guide
*/

// This function is used to initialize the pages that are 1 level deep in the quickstart html heirarchy
function qsInit( menuItem )
{
	var footerDiv = document.getElementById("footer");
	footerDiv.innerHTML = "<a href=\"../../index.html\">Home</a> | <a href=\"../css/sitemap.html\">Site Map</a> | " +
	"<a href=\"../css/contact_us.html\" onclick=\"window.open(this.href, '','height=300,width=400');return false;\">Contact Us</a> | "+
	"<a href=\"../css/cpyr.html\">Copyright &copy; 2018</a>";
	
	var leftpaneDiv = document.getElementById("leftpane");
	
	var toppaneDiv = document.getElementById("toppane");
	toppaneDiv.innerHTML = "<iframe id=\"topframe\" src=\"../main/top_index.html\" frameborder=\"0\" scrolling=\"No\"></iframe>"
	
	leftpaneDiv.innerHTML = "<iframe id=\"leftframe\" src=\"../main/nav.html\" frameborder=\"0\" scrolling=\"No\"></iframe>";
	//if (menuItem)
	//{
	//	leftpaneDiv.innerHTML = "<iframe id=\"leftframe\" src=\"../main/nav.html?menu=" + menuItem +
	//		"\" frameborder=\"0\" scrolling=\"No\"></iframe>";
	//}
	//else 
	//{
	//	leftpaneDiv.innerHTML = "<iframe id=\"leftframe\" src=\"../main/nav.html\" frameborder=\"0\" scrolling=\"No\"></iframe>";
	//}
}

// This function is used to initialize the pages that are 2 levels deep in the quickstart html heirarchy
function qsInit2()
{
	var footerDiv = document.getElementById("footer");
	footerDiv.innerHTML = "<a href=\"../../../index.html\">Home</a> | "+
	"<a href=\"../../css/sitemap.html\">Site Map</a> | " +
	"<a href=\"../../css/contact_us.html\" onclick=\"window.open(this.href, '','height=300,width=400');return false;\">Contact Us</a> | "+
	"<a href=\"../../css/cpyr.html\">Copyright &copy; 2015</a>";
	
	var leftpaneDiv = document.getElementById("leftpane");
	leftpaneDiv.innerHTML = "<iframe id=\"leftframe\" src=\"../../main/nav.html\" frameborder=\"no\" scrolling=\"No\" noresize=\"noresize\"></iframe>";
	
	var toppaneDiv = document.getElementById("toppane");
	toppaneDiv.innerHTML = "<iframe id=\"topframe\" src=\"../../main/top_index.html\" frameborder=\"no\" scrolling=\"No\" noresize=\"noresize\"></iframe>"
}

// This function is used to initialize tptbm popup html page with the chart showing TimesTen performance
function qsInitPopup()
{
	var footerDiv = document.getElementById("popupfooter");
	footerDiv.innerHTML = "<a href=\"../../index.html\">Home</a> | "+
	"<a href=\"../css/sitemap.html\">Site Map</a> | " +
	"<a href=\"../css/contact_us.html\" onclick=\"window.open(this.href, '','height=300,width=400');return false;\">Contact Us</a> | "+
	"<a href=\"../css/cpyr.html\">Copyright &copy; 2015</a>";
}

// This function is used to highlight the appropriate menu item in the navigation menu for every given page
function qsInitNav()
{
	var menuItem = getArg("menu");
	if (menuItem) 
	{
		document.getElementById(menuItem).className += " highlighted";
	}
}

// This function is used to get the name of the menu item to be highlighted in the navigation menu
function getArg( name )
{
  name = name.replace(/[\[]/,"\\\[").replace(/[\]]/,"\\\]");
  var regexS = "[\\?&]"+name+"=([^&#]*)";
  var regex = new RegExp( regexS );
  var results = regex.exec( window.location.href );
  if( results == null )
    return "";
  else
    return results[1];
}

// This is used to automatically generate breadcrumbs for the Quick Start guide
function generateBreadcrumbs()
{
	var path = "";
	var href = document.location.href;
	var s = href.split("/");
	for (var i=2;i<(s.length-1);i++) 
	{
		path+="<A HREF=\""+href.substring(0,href.indexOf("/"+s[i])+s[i].length+1)+"/\">"+s[i]+"</A> / ";
	}
	i=s.length-1;
	path+="<A HREF=\""+href.substring(0,href.indexOf(s[i])+s[i].length)+"\">"+s[i]+"</A>";
	var url = window.location.protocol + "//" + path;
	document.getElementById("breadcrumb").innerHTML += url + " ";
}

// The rest of the functions below are needed for loading images contained in the Java EE App servers html pages
var obeImages = new Array;
var eyeglass = new Image;

function preLoadImages()
{
  eyeglass.src = "images/view_image.gif";
  images = document.images;
// Only load images with class="imgborder_off"
  for (i = 0; i < images.length; i++)
  {
    image = images[i];
    if (image.className.substr(0, 10) == "imgborder_")
    {
        img = new Image;
        img.id = image.id;
        img.name = image.name;
        img.src = "images/" + image.name + ".gif";
        obeImages[img.name] = img;
    }
  }
}

function showImage(obj)
{
  if (obj.className.substr(0, 10) == "imgborder_")
  {
    newImg = obeImages[obj.name];
    obj.src = newImg.src;
    if (obj.border > 0)
    {
      obj.className = "imgborder_on";
    }
  }
}
 
function hideImage(obj)
{
  if (obj.className.substr(0, 10) == "imgborder_")
  {
    obj.src = eyeglass.src;
    obj.className = "imgborder_off";
  }
}
 
function showAllImages()
{
  imgs = document.images;
  for (i=0; i < imgs.length; i++)
  {
    showImage(imgs[i]);
  }
}
 
function hideAllImages()
{
  imgs = document.images;
  for (i=0; i < imgs.length; i++)
  {
    hideImage(imgs[i]);
  }
}
