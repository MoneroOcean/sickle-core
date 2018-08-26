{
    "targets": [
        {
            "target_name": "sickle-core",
            "sources": [
                "sickle-core.cpp",
            ],
            "include_dirs": [
                "<!(node -e \"require('nan')\")"
            ]
        }
    ]
}
