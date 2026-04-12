slangc colored_triangle.slang -target spirv -profile spirv_1_4 -o colored_triangle.spv
slangc gradient.slang -target spirv -profile spirv_1_4 -fvk-use-entrypoint-name -entry computeMain -o gradient.spv
