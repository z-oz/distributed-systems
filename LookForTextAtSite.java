import org.jsoup.Jsoup;
import org.jsoup.nodes.Document;
import org.jsoup.nodes.Element;
import org.jsoup.select.Elements;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.io.IOException;

public class LookForTextAtSite
{
	public static void main(String[] args)
	{
		String URL = args[0];
		int maxDepth = 0;
		int currDepth = 0;
		String searchString = args[2];

        if (Integer.parseInt(args[1]) > 0)
        {
        	maxDepth += Integer.parseInt(args[1]);
        }
        else
        {
        	System.out.println("Error: Please provide a nonNegInt!");
        	System.exit(1);
        }
        LookForTextAtSite finder = new LookForTextAtSite();
        List<String> URLs = new ArrayList<String>();
        finder.lookForText(URLs, URL, searchString, currDepth, maxDepth);
	}

	void lookForText(List<String> URLs, String URL, String searchString, int currDepth, int maxDepth)
	{
		try
		{
			if (currDepth <= maxDepth)
			{
				Document document = Jsoup.connect(URL).get();

				if (URLs.contains(URL))
				{
					return;
				}
				else
				{
					URLs.add(URL);
				}
				System.out.println("In " + URL);
				String textOfPage = document.select("*").text();
				lookForTextInPage(textOfPage, searchString);
				currDepth++;
				traverseTree(document, URLs, searchString, currDepth, maxDepth);
			}
		}
		catch (IOException e)
		{
			// TODO Auto-generated catch block
		}
	}

	void lookForTextInPage(String textOfPage, String searchString)
	{
		int firstOccurrence = textOfPage.indexOf(searchString);

		if (firstOccurrence >= 0)
		{
			int start = 0;
			int end = textOfPage.length() - 1;

			if (firstOccurrence - 64 > 0)
			{
				start = firstOccurrence - 64;
			}
			if (firstOccurrence + 64 < end)
			{
				end = firstOccurrence + 64 + searchString.length();
			}
			String context = textOfPage.substring(start, end);
			System.out.println(context);
		}
	}

	boolean pageFetcher(String href)
	{
		boolean newLink = false;
		if (href.length() >= 1)
		{
			if (href.startsWith("http") || href.startsWith("https"))
			{
				int start = 0;
				if (href.startsWith("http"))
				{
					start = href.indexOf("http") + 7;
				}
				else
				{
					start = href.indexOf("https") + 8;					// https://repl.it/languages/java
				}
				String baseAndAfter = href.substring(start);			// /repl.it/languages/java
				int slash = baseAndAfter.lastIndexOf("/");

				if (slash == -1 || slash == baseAndAfter.length() - 1)
				{
					return newLink = true;
				}
				else
				{
					String page = baseAndAfter.substring(slash);		// /java
					int extStart = page.lastIndexOf(".");

					if (extStart == -1)
					{
						return newLink = true;
					}
					else
					{
						String ext = page.substring(extStart);	// .it - from baseAndAfter being "/repl.it"

						if (ext.equals(".html") || ext.equals(".htm") || ext.equals(".txt")) // || ext.equals("")
						{
							return newLink = true;
						}
					}
				}
			}
		}
		return newLink;
	}

	void traverseTree(Document document, List<String> URLs, String searchString, int currDepth, int maxDepth)
	{
		ArrayList<String> arr = new ArrayList<String>();
		Elements URLs1 = document.select("a");
		for (Element URL : URLs1)
		{
			String href = URL.attr("abs:href");
			boolean pageFetcher = pageFetcher(href);
			if (maxDepth > 0 && pageFetcher)
			{
				arr.add(href);
			}
		}
		for(String str : arr)
		{
			lookForText(URLs, str, searchString, currDepth, maxDepth);
		}
	}

}
