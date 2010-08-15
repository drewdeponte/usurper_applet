MAIN_BIN=usurper-applet
MAIN_SRC=usurper-applet.c
MAIN_HDR=usurper-applet.h
SERVER_FILE=UsurperApplet_Factory.server

all: $(MAIN_BIN)

$(MAIN_BIN) : $(MAIN_SRC)
	gcc `pkg-config --cflags --libs libpanelapplet-2.0` `pkg-config --cflags --libs poppler-glib` -g -o $(MAIN_BIN) $(MAIN_SRC)

install : $(MAIN_BIN) $(SERVER_FILE)
	cp $(MAIN_BIN) /usr/lib/gnome-panel/
	cp $(SERVER_FILE) /usr/lib/bonobo/servers/

clean :
	rm -rf $(MAIN_BIN)
