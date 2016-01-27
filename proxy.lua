--cdb = require("cdb")
--rex = require("rex_pcre")

function get_backend_from_username(name)
   return name..".sql.example.net", "3306"
end
