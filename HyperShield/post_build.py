import httplib
import sys, os
def getData(path, name):
    file_name = path + name
    try:
        with open( file_name, 'r' ) as n:
            pass
    except:
        print 'NO RESOURCE FILE ...'
        print 'Now download .. .. ..'
        r = httplib.HTTPConnection('redbit.dothome.co.kr')
        r.request('GET', name)
        r = r.getresponse()
        d = r.read()

        with open( file_name, 'wb' ) as f:
            f.write( d )


if __name__ == '__main__':
    path = sys.argv[1] + '/resources'
    print path
    if not os.path.isdir( path ):
        os.mkdir( path )
    getData( path, '/bg.bmp' )
    getData( path, '/bgm.wav' )
    sys.exit(0)
