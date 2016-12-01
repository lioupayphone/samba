#!/bin/sh
tmp_dir="/var/run/ctdb"
cache_file="$tmp_dir/netcache.$$"

reload_smb()
{
	service smb status >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		return 0
	fi
	smbcontrol smbd reload-config >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "failed to reload smb service."
		return 1
	fi
	
	return 0
}

reload_nfs()
{
	service nfs-ganesha status >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		return 0
	fi
	
	service nfs-ganesha reload >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "failed to reload nfs service."
		return 1
	fi
	return 0
}

reload_dns()
{
	service dnsmasq status >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		return 0
	fi
	service dnsmasq restart >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "failed to reload dnsmasq service."
		return 1
	fi
	
	return 0
}
pull_smb()
{
	tmp_smb_file="$tmp_dir/smb.conf.$$"
	touch $tmp_smb_file >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "failed to create temp file for pull smb config."
		return 1
	fi
	
	echo "begin to....."
	rm -f $cache_file >/dev/null 2>&1
	content=$(net registry enumerate "HKLM\Software\Samba\smbconf")
	if [ $? -ne 0 ] ||[  x"$content" == x ] ; then
		echo "Failed to read smb configuration from registry.tdb."
		return 1
	fi
	for service in ` net registry enumerate 'HKLM\Software\Samba\smbconf' |grep "Keyname"|awk -F= '{print $2}' |sed 's/^[ ]*//g'`
	do
		echo "service is $service"
		echo "[$service]" >> $tmp_smb_file

		net registry enumerate "HKLM\Software\Samba\smbconf\\$service" |grep "Valuename" |awk -F= '{print $2}'|sed 's/^[ ]*//g' > $cache_file 2>/dev/null
		
		while read valuename
		do
			echo "valuename is $valuename"
			value=$(net registry getvalueraw "HKLM\Software\Samba\smbconf\\$service" "$valuename")
			if [ $? -eq 0 ] ; then
				echo "$valuename = $value" >> $tmp_smb_file
			fi
		done < $cache_file
	
	done
	
	mv $tmp_smb_file /etc/samba/smb.conf
	if [ $? -ne 0 ] ; then
		echo "failed to override smb.conf."
		rm -rf $tmp_smb_file >/dev/null 2>&1
		rm -rf $cache_file >/dev/null 2>&1
		return 1
	fi
	rm -rf $tmp_smb_file >/dev/null 2>&1
	rm -rf $cache_file >/dev/null 2>&1
	reload_smb
	if [ $? -ne 0 ] ; then
		return 1
	fi
	return 0
}
pull_nfs()
{
	tmp_nfs_file="$tmp_dir/gluster.ganesha.conf.$$"
	touch $tmp_nfs_file >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "failed to create temp for pull nfs config."
		return 1
	fi
	
	rm -rf $cache_file >/dev/null 2>&1
	content=$(net registry enumerate "HKLM\Software\nfs")
	if [ $? -ne 0 ] ; then
		echo "Failed to read nfs configuration from registry.tdb."
		return 1
	fi
	
	for export_id in `net registry enumerate 'HKLM\Software\nfs' |grep "Keyname"|awk -F= '{print $2}' |sed 's/^[ ]*//g'`
	do
		echo "EXPORT" >> $tmp_nfs_file
		echo "{"      >> $tmp_nfs_file
		net registry enumerate "HKLM\Software\nfs\\$export_id" |grep "Valuename" |awk -F= '{print $2}'|sed 's/^[ ]*//g' > $cache_file 2>/dev/null
		while read valuename
		do
			if [ x"$valuename" == x"Name" -o x"$valuename" == x"Hostname" -o x"$valuename" == x"Volume" ] ; then
				continue
			fi
			value=$(net registry getvalueraw "HKLM\Software\nfs\\$export_id" "$valuename")
			if [ $? -eq 0 ] ; then
				echo " $valuename = $value" >> $tmp_nfs_file
			fi
		done < $cache_file
		
		echo "FSAL {" >> $tmp_nfs_file
		
		value=$(net registry getvalueraw "HKLM\Software\nfs\\$export_id" Name)
		if [ $? -eq 0 ] ; then
			echo "    Name = $value" >> $tmp_nfs_file
		fi
		
		value=$(net registry getvalueraw "HKLM\Software\nfs\\$export_id" Hostname)
		if [ $? -eq 0 ] ; then
			echo "    Hostname = $value" >> $tmp_nfs_file 
		fi
		
		value=$(net registry getvalueraw "HKLM\Software\nfs\\$export_id" Volume)
		if [ $? -eq 0 ] ; then
			echo "    Volume = $value" >> $tmp_nfs_file
		fi
		echo " }" >> $tmp_nfs_file
		echo "}" >> $tmp_nfs_file
	done
	
	mv $tmp_nfs_file /etc/ganesha/gluster.ganesha.conf
	if [ $? -ne 0 ] ; then
		echo "Failed to override nfs configuration."
		rm -rf $tmp_nfs_file >/dev/null 2>&1
		rm -rf $cache_file >/dev/null 2>&1
		return 1
	fi
	
	rm -rf $tmp_nfs_file >/dev/null 2>&1
	rm -rf $cache_file >/dev/null 2>&1
	reload_nfs
	if [ $? -ne 0 ] ; then
		return 1
	fi
	
	return 0
	
}


pull_dns()
{
	tmp_dns_file="$tmp_dir/zone.conf.$$"
	touch $tmp_dns_file >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "failed to create file $tmp_dns_file."
		return 1
	fi
	
	rm -f $cache_file >/dev/null 2>&1
	content=$(net registry enumerate "HKLM\Software\dns")
	if [ $? -ne 0 ] ; then
		echo "Failed to read dns configuration from registry.tdb."
		return 1;

	fi	

	for zone in ` net registry enumerate 'HKLM\Software\dns' |grep "Keyname"|awk -F= '{print $2}' |sed 's/^[ ]*//g'`
	do
		echo "zone is $zone"
		echo "zone $zone" >> $tmp_dns_file
		net registry enumerate "HKLM\Software\dns\\$zone" |grep "Valuename" |awk -F= '{print $2}'|sed 's/^[ ]*//g' > $cache_file 2>/dev/null	
	
		while read valuename
		do
			echo "valuename is $valuename"
			value=$(net registry getvalueraw "HKLM\Software\dns\\$zone" "$valuename")
			if [ $? -eq 0 ] ; then
				echo " $valuename $value" >> $tmp_dns_file
			fi
		done < $cache_file


		echo "end zone" >> $tmp_dns_file
	done
	
	mv $tmp_dns_file /etc/zone.conf
	if [ $? -ne 0 ] ; then
		echo "failed to override zone.conf."
		rm -rf $tmp_dns_file >/dev/null 2>&1
		rm -rf $cache_file >/dev/null 2>&1
		return 1
	fi
	rm -rf $tmp_dns_file >/dev/null 2>&1
	rm -rf $cache_file >/dev/null 2>&1
	reload_dns
	if [ $? -ne 0 ] ; then
		return 1
	fi
	
	return 0
}

