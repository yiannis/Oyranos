static const char help_message[] = ""
" +-------------------------------------------------------------------------------------------------------------------------------------------+\n"
" |                                       Get various cinds of information about installed SANE Devices                                       |\n"
" |-------------------------------------------------------------------------------------------------------------------------------------------|\n"
" |        |                  INPUT                   |                                        OUTPUT                                         |\n"
" |--------+------------------------------------------+---------------------------------------------------------------------------------------|\n"
" |        |     tag      |    value     |  Comments  |  oyConfig_s  |      tag      |      value       |               Action                |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |required|command       |list(*)       |expensive   |::backend_core|device_name    |<string>          |List Pluged-In Devices, each into an |\n"
" |        |              |              |call        |              |               |                  |oyConfig_s struct                    |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |        |              |              |            |::rank_map    |               |                  |Copy only the static map             |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |optional|driver_version|<int>         |(***)       |::data        |driver_version |<int>             |                                     |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |optional|oyNAME_NAME   |              |            |::data        |model???       |e.g CanoScan      |Short text description of            |\n"
" |        |              |              |            |              |               |N670U/N676U/LiDE20|device/lightweight information       |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |optional|device_name   |<string>      |As returned |              |               |                  |Use only specified device            |\n"
" |        |              |              |from “list” |              |               |                  |                                     |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |        |              |              |Important if|              |               |                  |                                     |\n"
" |optional|device_context|(****)        |using also  |::backend_core|device_context |SANE_Device*      |Pointer to SANE struct, lifetime     |\n"
" |        |              |              |“propeties” |              |               |(oyBlob_s)        |until next call to sane_get_devices()|\n"
" |        |              |              |call        |              |               |                  |                                     |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |optional|device_handle |              |sane_open() |::backend_core|device_handle  |SANE_Handle       |Get a handle to the scanner device.  |\n"
" |        |              |              |is called   |              |               |(oyBlob_s)        |Lifetime until sane_close()          |\n"
" |-------------------------------------------------------------------------------------------------------------------------------------------|\n"
" |                                              Get the properties of a particular SANE device                                               |\n"
" |-------------------------------------------------------------------------------------------------------------------------------------------|\n"
" |        |                  INPUT                   |                                        OUTPUT                                         |\n"
" |--------+------------------------------------------+---------------------------------------------------------------------------------------|\n"
" |        |     tag      |    value     |  Comments  |  oyConfig_s  |      tag      |      value       |               Action                |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |        |              |              |            |              |               |                  |List Pluged-In Devices H/W Properties|\n"
" |required|command       |properties(**)|            |::backend_core|<property name>|<string>          |and also color options from sane     |\n"
" |        |              |              |            |              |               |                  |driver                               |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |        |              |              |            |              |               |                  |Copy the full color related rank map |\n"
" |        |              |              |            |::rank_map    |               |                  |by querying the SANE driver. Only    |\n"
" |        |              |              |            |              |               |                  |when device_* is present             |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |required|device_name   |<string>      |As returned |::backend_core|device_name    |<string>          |Use only the specified device        |\n"
" |        |              |              |from “list” |              |               |                  |                                     |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |optional|device_context|SANE_Device   |Highly      |              |               |                  |Call sane_get_devices() if not       |\n"
" |        |              |              |desirable   |              |               |                  |supplied [expensive]                 |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |        |              |              |            |              |               |SANE_Handle       |If value==null, supply a new         |\n"
" |optional|device_handle |SANE_Handle   |            |::backend_core|device_handle  |(oyCMMptr_s)      |SANE_Handle. User has to call        |\n"
" |        |              |              |            |              |               |                  |sane_close(sane_handle) herself      |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |optional|driver_version|<int>         |            |::backend_core|driver_version |<int>             |                                     |\n"
" |-------------------------------------------------------------------------------------------------------------------------------------------|\n"
" |                                                       Get an extensive help mesage                                                        |\n"
" |-------------------------------------------------------------------------------------------------------------------------------------------|\n"
" |        |                  input                   |                                        output                                         |\n"
" |--------+------------------------------------------+---------------------------------------------------------------------------------------|\n"
" |        |     tag      |    value     |  Comments  |  oyConfig_s  |      tag      |      value       |               Action                |\n"
" |--------+--------------+--------------+------------+--------------+---------------+------------------+-------------------------------------|\n"
" |required|command       |help          |            |              |               |                  |Print help message to standard error |\n"
" +-------------------------------------------------------------------------------------------------------------------------------------------+\n"
;
