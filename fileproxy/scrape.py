from bs4 import BeautifulSoup
import urlparse
import sys

numargs = len(sys.argv)
if numargs < 4:
    print 'Usage: python scrape.py input_file output_file base_url'

inputfile = sys.argv[1]
output = sys.argv[2]
base_url = sys.argv[3]
outfile = open(output, 'w+')

if not (base_url.startswith('http')):
    base_url = 'http://' + base_url

f = open(inputfile)
data = f.read()

soup = BeautifulSoup(data)

relative = ''
for link in soup.find_all('a'):
    if link.has_attr('href'):
        rel_link = link['href']
        if rel_link.startswith('http'):
           relative = rel_link 
        else:
            relative = urlparse.urljoin(base_url, rel_link)
           
        hostname = urlparse.urlparse(relative).hostname
        path = urlparse.urlparse(relative).path

        if hostname == None:
            hostname = ''
        if path == None:
            path = ''
        
        out = hostname + ' ' + path + '\n'
        outfile.write(out)

outfile.close
