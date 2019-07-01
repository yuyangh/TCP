from time import gmtime, strftime
import time

def running():
    print("always running")
    print(strftime("%a, %d %b %Y %H:%M:%S +0000", gmtime()))
    while 1!=2:
        pass


if __name__ == "__main__":
    running()