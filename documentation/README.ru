1. ��������

Wine - ��� ���������, ������� ��������� ��������� ��������� Microsoft
Windows (������� DOS, Windows 3.x � Win32) �� Unix. ��� ������� ��
��������� ��������, ������� ��������� � ��������� ��������� Microsoft
Windows, � ���������� (Winelib), ������� ��������� ������ Windows API,
��������� �� Unix- ��� X11-�����������. ���������� ����� ����� ����
������������ ��� ������������ ���� Win32 � ����� Unix.

Wine - ��������� ����������� �����������, �������������� �� ��������
GNU LGPL; �������� ���� LICENSE.


2. ������� �����

������ ���, ����� �� ������������ �������� ���, ������������� ������������
Wine Installer ��� ���������� � ��������� Wine. � ��������, ����������
�������� ��� Wine (� ��� ��������� ���� ����), �������:

./tools/wineinstall

��������� ��������� ��� "wine program". ��������� �������������� ����������
����������� � ����� ����� �����. ������������ � ������������ �� Wine,
� ����� � ������ http://www.winehq.org.


3. ����������

���������� � ������ Wine �������������� � ��������� ������������ ��������:

  Linux ������ 2.0.36 ��� ����� �����
  FreeBSD 7.0 ��� ����� �����
  Solaris x86 9 ��� ����� �����
  NetBSD-������� ������
  Mac OS X 10.4 ��� ����� �����

��� ��� Wine ������� ��� ������� ��������� ������� �� ������ ����, ��������������
������ ������������ �������, ������������� ����.  ������ ������������ �������,
� ���������� ��������������� �����, ����� �������������� Wine � �������.

Linux:
  Linux 2.2.x: ������ ��������; Linux 2.0.x: ����� ��������
  (������ ������ 2.0.x ������ �������� ���� �������),
  ����� ������������ ������� ������ ���� 2.4.x ��� 2.6.x.

FreeBSD:
  Wine �� ����� �������� ��� ������� �� ������� ����, ��� FreeBSD
  7.0. � FreeBSD 6.3 ���� ����, ����������� Wine �����������. ��������
  <http://wiki.freebsd.org/Wine> ��� �������������� ����������.

Solaris:
  ����� ����� ����������� Wine ������������� GNU
  (gcc, gas, � �.�.). ��������: ��������� gas *��* �����������
  ��� gcc ����� ��� ������������. ���������� ����������������� gcc �����
  ��������� gas ��� ������� ������ �� cc, ��� � �� ld � ������������ gnu. (?)

NetBSD:
  ��������� ���, ��������� USER_LDT, SYSVSHM, SYSVSEM � SYSVMSG
  �������� � ����� ����.

Mac OS X:
  ��� ����� Xcode 2.4 ��� ����� ����� ��� ���������� x86.


�������������� �������� �������:
  Wine ������ �������� �� ����������� �������� ��������. ������� ����������
  ������������� ���� �������� ��� ������������� ������ ����� Samba. �����,
  NTFS �� ������������� ���� ������������ �������� �������, ����������� ���������
  �����������.  ������������� ������������� �������� ������ Linux, ����� ��� ext3.

������� ����������:
  � ��� ������ ���� ����������� ������������ ����� X11
  (xlib6g-dev � Debian � XFree86-devel � Red Hat).

  �������, ��� ��������� "make" (����� ����� GNU make).

  ��� ����� ����� flex ������ 2.5 ��� �����, � ����� bison.

��������� �������������� ���������:
  ���������������� ������ ������� ������ �������������� ���������, ������� �� ����
  ������� � ����� �������. �������� http://wiki.winehq.org/Recommended_Packages ���
  ������ �������, ������� �� ������ ����������.

  �� 64-������ ���������� �� ������ ���������, ��� � ��� ����������� 32-������
  ������ ���� ���������; �� ����� ��������� ����������� ���������� �
  http://wiki.winehq.org/WineOn64bit.

4. ����������

���� �� �� ������ ������������ wineinstall, ������� ��������� �������
��� ���������� Wine:

./configure
make depend
make

��� ���������� ��������� "wine" � �������������� ����������/�������� ����� ���������.
��������� "wine" ����� ���������, � ��������� ��������� Windows.
���������� "libwine" ("Winelib") ����� ���� ������������ ��� �������������� �
���������� ��������� ���� Windows ��� Unix.

��� ������ ���������������� ��������, ������� ./configure --help.

��� ���������� �� ������ ������ ��������� ����, ������� ��������� �
�������� ������� ������ (�� �������� README ����).
����� ������� "make clean", � ��������� �����:

    bunzip2 -c patch-file | patch -p1

��� "patch-file" - ��� ����� � ������ (���-�� �����
wine-1.0.x.diff.bz2). ����� ������� "./configure", � ����� �����
"make depend && make".


5. ���������

����� ���� ���, Wine ����� ������� �����������, ������� "make install"; ���
��������� ����������� ����� wine, ����������� Wine, � ���������
����������� �����.

������� �� �������� ������� ����� ������������� ���������� ��������� Wine.
���������� "dpkg -r wine" ��� "rpm -e wine" ��� "make uninstall"
�� ���������.

����� ���������, ��������� ���������������� ��������� "winecfg". ��������
������ ��������� �� http://www.winehq.org/ ��� �������������� ��������.


6. ������ ��������

�� ������ ��������� ���� �� �����, ��� ������ ��� �����.

��������: ������ ��������:

	wine notepad		   (��������� ����, �����������
	wine notepad.exe	    � �������, ��� ���������� �����)

	wine c:\\windows\\notepad.exe  (��������� ��������� DOS)

	wine ~/.wine/drive_c/windows/notepad.exe  (��������� ��������� Unix)

        wine notepad.exe /parameter1 -parameter2 parameter3
				   (����� ��������� � �����������)

Wine ��������� � �������� ����������, ������� ��������� ��������� �����
�������� ������. �� ����� ������ �� ������ ���������� � �������� ��� ����,
����� �� ����� ����������� � ������ ��������. ��� ��������� ����������,
�������� ����� ������� � Wine Developer's Guide.


7. ��������� �������������� ����������

WWW:	 ����� ���������� � Wine �������� � WineHQ ��
	 http://www.winehq.org/: ��������� ����������� Wine, ���� ������ ����������,
	 ���� ������ ������. �������� ����� ����� ������ � ����� �����.

FAQ:	 Wine FAQ ���������� �� http://www.winehq.org/FAQ

Usenet:	 �� ������ �������� ������� Wine � �������� ������
	 �� comp.emulators.ms-windows.wine.

������:	 ��������� �� ������� � Wine Bugzilla �� http://bugs.winehq.org
	 ������ ��� ���������� ��������� �� ������ ���������, ����� ��� ������
	 ��� �������. �� ����� ������ ���������� ��������� �� ������� ��
         comp.emulators.ms-windows.wine.

IRC:	 ������ ������ �������� �� ������ #WineHQ �� irc.freenode.net.

GIT:	 ������� ���������� Wine �������� ����� GIT.
	 �������� http://www.winehq.org/site/git ��� �������������� ����������.

��������:
	 ������� ��������� �������� ��� ������������� � ������������� Wine;
	 �������� http://www.winehq.org/forums ��� �������������� ����������.

Wiki:	 Wine Wiki ���������� �� http://wiki.winehq.org

���� �� ���-�� ��������, ��� ��������� ������, �������� ���� (� ������� 'diff -u')
�� wine-patches@winehq.org ��� ��������� ��� � ��������� �����.

--
Alexandre Julliard
julliard@winehq.org

�������� �� ������� ����
�������� ���������
scriptkid@mail.ru
������� �������
lav@etersoft.ru
