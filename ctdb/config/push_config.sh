#!/bin/sh

smb_file="/etc/samba/smb.conf"
zone_file="/etc/zone.conf"
nfs_file="/etc/ganesha/gluster.ganesha.conf"


push_smb()
{
	net conf import $smb_file >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "Failed to push smb configuration into registry.tdb"
		return 1
	fi
	
	return 0
}

push_nfs()
{
	net registry deletekey_recursive 'HKLM\Software\nfs' >/dev/null 2>&1
	
	net registry createkey 'HKLM\Software\nfs' >/dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "Failed to create nfs key."
		return 1
	fi
	
	name=""
	value=""
	export_id=""
	while read line
	do
		echo $line |egrep "^\s*#" >/dev/null 2>&1
		if [ $? -eq 0 ] ; then
			continue
		fi
		
		echo $line |egrep "^\s*\w+\s*=" >/dev/null 2>&1
		if [ $? -eq 0 ] ; then
			echo $line |egrep "^\s*Export_Id\s*=" >/dev/null 2>&1
			if [ $? -eq 0 ] ; then
				export_id=`echo $line|awk -F= '{print $2}'|sed 's/^[ \t]*//g'`
				net registry createkey "HKLM\Software\nfs\\$export_id" >/dev/null 2>&1
				if [ $? -ne 0 ] ; then
					echo "Failed to create nfs key $export_id"
					return 1
				fi
			fi
			if [ x"$export_id" != x ] ;  then
				name=`echo $line|awk -F= '{print $1}'|sed 's/^[ \t]*//g'|sed 's/[ \t]*$//g'`
				value=`echo $line|awk -F= '{print $2}'|sed 's/^[ \t]*//g'`
				net registry setvalue "HKLM\Software\nfs\\$export_id" $name sz $value >/dev/null 2>&1
				if [ $? -ne  0 ] ; then
					echo "Failed to set value for $export_id $name $value"
					return 1
				fi
			fi
		
		else
			continue
		fi
			
	done < $nfs_file
	
	return 0
}

push_dns()
{
	net registry deletekey_recursive 'HKLM\Software\dns' >/dev/null 2>&1
	
	net registry createkey 'HKLM\Software\dns' >/dev/null 2>&1
	if [ $? -ne 0 ] ; then	
		echo "failed to create dns key."
		return 1	
	fi
	name=""
	value=""
	while read line 
	do
		#echo "line is $line"
		echo $line|egrep "^zone" >/dev/null 2>&1
		if [ $? -eq 0 ] ; then
		
			name=`echo $line | awk '{print $2}'`
			#echo "name is $name"
			net registry createkey "HKLM\Software\dns\\$name" >/dev/null 2>&1
			if [ $? -ne 0 ] ; then			
				echo "failed to create dns key $name"
				return 1
			fi
		fi
		
		echo $line |egrep "^\s*policy" >/dev/null 2>&1
		if [ $? -eq 0 ] ; then
		
			value=`echo $line |awk '{print $2}'`
			echo "value is $value"
			net registry setvalue "HKLM\Software\dns\\$name" policy sz "$value" >/dev/null 2>&1
			if [ $? -ne 0 ] ; then
				echo "failed to set policy $value"
				return 1
			fi
		fi
		
		echo $line |egrep "^\s*iplist" >/dev/null 2>&1
		if [ $? -eq 0 ] ; then
		
			value=`echo $line |awk '{$1="";print $0}'|sed 's/^[ \t]*//g'`
			echo "iplist is $value"
			net registry setvalue "HKLM\Software\dns\\$name" iplist sz "$value" >/dev/null 2>&1
			if [ $? -ne 0 ] ; then
				echo "failed to set iplist $value"
				return 1
			fi
		fi

                echo $line |egrep "^\s*type" >/dev/null 2>&1
                if [ $? -eq 0 ] ; then
		        value=`echo $line | awk '{print $2}'`
                        echo "type is $value"
                        net registry setvalue "HKLM\Software\dns\\$name" type sz "$value" >/dev/null 2>&1
                        if [ $? -ne 0 ] ; then
                                 echo "failed to set type $value"
                                 return 1
                        fi
                fi

		echo $line |egrep "^end" >/dev/null 2>&1
		if [ $? -eq 0 ] ; then		
			continue
		fi	
		
	done < $zone_file
	return 0
	
}
