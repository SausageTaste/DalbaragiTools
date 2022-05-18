#include "daltools/model_parser.h"

#include <nlohmann/json.hpp>

#include "daltools/byte_tool.h"
#include "daltools/konst.h"
#include "daltools/compression.h"


namespace dalp = dal::parser;


namespace dal::parser {

	JsonParseResult parse_json(SceneIntermediate& scene, const uint8_t* const file_content, const size_t content_size) {
		const auto json_data = nlohmann::json::parse(file_content, file_content + content_size);

		return JsonParseResult::success;
	}

}
