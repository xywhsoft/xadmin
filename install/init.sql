--
-- SQLiteStudio v3.4.17 生成的文件，周五 4月 3 18:52:48 2026
--
-- 所用的文本编码：UTF-8
--
PRAGMA foreign_keys = off;
BEGIN TRANSACTION;

-- 表：attachment
CREATE TABLE attachment (
    xid           TEXT    PRIMARY KEY,
    filename      TEXT,
    ext           TEXT,
    mime          TEXT,
    size          INTEGER DEFAULT (0),
    path          TEXT,
    modelName     TEXT    DEFAULT '',
    recordId      INTEGER DEFAULT (0),
    uploaderId    INTEGER DEFAULT (0),
    uploaderType  INTEGER DEFAULT (1),
    allowHotlink  INTEGER DEFAULT (1),
    accessType    INTEGER DEFAULT (0),
    accessLevel   INTEGER DEFAULT (0),
    price         INTEGER DEFAULT (0),
    priceType     INTEGER DEFAULT (0),
    salesCount    INTEGER DEFAULT (0),
    downloadCount INTEGER DEFAULT (0),
    remark        TEXT    DEFAULT '',
    createTime    INTEGER,
    isDelete      INTEGER DEFAULT (0) 
);


-- 表：attachmentOrder
CREATE TABLE attachmentOrder (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    attachmentXid TEXT,
    memberId      INTEGER,
    price         INTEGER DEFAULT (0),
    priceType     INTEGER DEFAULT (0),
    sellerId      INTEGER DEFAULT (0),
    sellerIncome  INTEGER DEFAULT (0),
    createTime    INTEGER
);


-- 表：auth
CREATE TABLE auth (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    groupID            INTEGER,
    name               TEXT,
    desc               TEXT,
    sort               INTEGER,
    createTime         INTEGER,
    updateTime         INTEGER,
    isDelete           INTEGER DEFAULT (0),
    plugin_id          TEXT,
    plugin_instance_id TEXT,
    plugin_generation  INTEGER,
    plugin_xid         TEXT
);

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     1,
                     1,
                     '未分组',
                     '未分组的 URI 会被归类到这里',
                     0,
                     63930019316,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     2,
                     2,
                     '基础页面',
                     '后台登录、主页等基础功能页面和接口',
                     100000,
                     63933903210,
                     63942459127,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     3,
                     3,
                     '附件列表',
                     '[附件管理] - [附件列表] 页面和接口',
                     200000,
                     63936000000,
                     63942459180,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     4,
                     3,
                     '存储统计',
                     '[附件管理] - [存储统计] 页面和接口',
                     201000,
                     63936000000,
                     63942459187,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     5,
                     4,
                     '用户管理',
                     '[前台用户管理] - [用户管理] 页面和接口',
                     300000,
                     63934949282,
                     63942459200,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     6,
                     4,
                     '用户组管理',
                     '[前台用户管理] - [用户组管理] 页面和接口',
                     301000,
                     63934949319,
                     63942459204,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     7,
                     4,
                     '权限分类',
                     '[前台用户管理] - [权限分类] 页面和接口',
                     302000,
                     63934949844,
                     63942459208,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     8,
                     4,
                     '权限分组',
                     '[前台用户管理] - [权限分组] 页面和接口',
                     303000,
                     63934949844,
                     63942459212,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     9,
                     5,
                     '用户管理',
                     '[后台权限管理] - [用户管理] 页面和接口',
                     400000,
                     63933903342,
                     63942459217,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     10,
                     5,
                     '角色管理',
                     '[后台权限管理] - [角色管理] 页面和接口',
                     401000,
                     63933903366,
                     63942459222,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     11,
                     5,
                     '权限分类',
                     '[后台权限管理] - [权限分类] 页面和接口',
                     402000,
                     63933903387,
                     63942459226,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     12,
                     5,
                     '权限分组',
                     '[后台权限管理] - [权限分组] 页面和接口',
                     403000,
                     63933903428,
                     63942459231,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     13,
                     6,
                     '插件商店',
                     '[插件管理] - [插件商店] 页面和接口',
                     500000,
                     63942459644,
                     63942459644,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     14,
                     6,
                     '已安装插件',
                     '[插件管理] - [已安装插件] 页面和接口',
                     501000,
                     63942459785,
                     63942459785,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     15,
                     7,
                     '设置管理',
                     '[设置] 自定义配置页面和接口',
                     600000,
                     63934948478,
                     63942459650,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     16,
                     7,
                     '菜单管理',
                     '[设置] - [菜单管理] 页面和接口',
                     601000,
                     63934948510,
                     63942459678,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     17,
                     7,
                     '接口管理',
                     '[设置] - [接口管理] 页面和接口',
                     602000,
                     63933903456,
                     63942459655,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     18,
                     8,
                     '系统日志',
                     '[系统日志] 页面和接口',
                     700000,
                     63933903308,
                     63942459671,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     19,
                     9,
                     '二次开发接口',
                     '用于二次开发的页面和接口',
                     800000,
                     63942459715,
                     63942459796,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     20,
                     10,
                     '调试接口',
                     '用于开发调试的页面和接口',
                     900000,
                     63942459744,
                     63942459804,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );


-- 表：authGroup
CREATE TABLE authGroup (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    name               TEXT,
    desc               TEXT,
    sort               INTEGER,
    createTime         INTEGER,
    updateTime         INTEGER,
    isDelete           INTEGER DEFAULT (0),
    plugin_id          TEXT,
    plugin_instance_id TEXT,
    plugin_generation  INTEGER,
    plugin_xid         TEXT
);

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          1,
                          '未分类',
                          '未分类的权限组会归到这里',
                          0,
                          63930028060,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          2,
                          '基础权限',
                          '后台登录、主页等基础功能',
                          100000,
                          63933903097,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          3,
                          '附件管理',
                          '[附件管理] 菜单下的功能',
                          200000,
                          63936000000,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          4,
                          '前台用户管理',
                          '[前台用户管理] 菜单下的功能',
                          300000,
                          63934948299,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          5,
                          '后台权限管理',
                          '[后台权限管理] 菜单下的功能',
                          400000,
                          63933903052,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          6,
                          '插件管理',
                          '[插件管理] 菜单下的功能',
                          500000,
                          63942458755,
                          63942458755,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          7,
                          '设置',
                          '[设置] 菜单下的功能',
                          600000,
                          63934948202,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          8,
                          '系统日志',
                          '[系统日志] 菜单下的功能',
                          700000,
                          63933903025,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          9,
                          '二次开发',
                          '用于正式的二次开发的接口',
                          800000,
                          63933903025,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete,
                          plugin_id,
                          plugin_instance_id,
                          plugin_generation,
                          plugin_xid
                      )
                      VALUES (
                          10,
                          '调试接口',
                          '用于开发调试的接口',
                          900000,
                          63934949629,
                          63936000000,
                          0,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                      );


-- 表：logs
CREATE TABLE logs (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user       TEXT,
    ip         TEXT,
    uri        TEXT,
    method     TEXT,
    param      TEXT,
    body       TEXT,
    createTime INTEGER
);


-- 表：member
CREATE TABLE member (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    username   TEXT    UNIQUE,
    salt       TEXT,
    pwd        TEXT,
    groupId    INTEGER DEFAULT (1),
    authLevel  INTEGER DEFAULT (0),
    balance    INTEGER DEFAULT (0),
    nickname   TEXT,
    email      TEXT,
    phone      TEXT,
    avatar     TEXT,
    status     INTEGER DEFAULT (1),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);


-- 表：memberAuth
CREATE TABLE memberAuth (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    groupID            INTEGER DEFAULT (1),
    name               TEXT,
    desc               TEXT,
    sort               INTEGER DEFAULT (0),
    createTime         INTEGER,
    updateTime         INTEGER,
    isDelete           INTEGER DEFAULT (0),
    plugin_instance_id TEXT,
    plugin_generation  INTEGER,
    plugin_xid         TEXT
);

INSERT INTO memberAuth (
                           id,
                           groupID,
                           name,
                           desc,
                           sort,
                           createTime,
                           updateTime,
                           isDelete,
                           plugin_instance_id,
                           plugin_generation,
                           plugin_xid
                       )
                       VALUES (
                           1,
                           1,
                           '未分类',
                           '未分类的接口',
                           0,
                           63942460323,
                           63942460323,
                           0,
                           NULL,
                           NULL,
                           NULL
                       );

INSERT INTO memberAuth (
                           id,
                           groupID,
                           name,
                           desc,
                           sort,
                           createTime,
                           updateTime,
                           isDelete,
                           plugin_instance_id,
                           plugin_generation,
                           plugin_xid
                       )
                       VALUES (
                           2,
                           2,
                           '基础接口',
                           '登录、注册、个人中心等基础接口',
                           100,
                           63935000000,
                           63942460329,
                           0,
                           NULL,
                           NULL,
                           NULL
                       );

INSERT INTO memberAuth (
                           id,
                           groupID,
                           name,
                           desc,
                           sort,
                           createTime,
                           updateTime,
                           isDelete,
                           plugin_instance_id,
                           plugin_generation,
                           plugin_xid
                       )
                       VALUES (
                           3,
                           3,
                           'VIP功能',
                           'VIP专属功能接口',
                           200,
                           63935000000,
                           63942460333,
                           0,
                           NULL,
                           NULL,
                           NULL
                       );


-- 表：memberAuthGroup
CREATE TABLE memberAuthGroup (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    name               TEXT,
    desc               TEXT,
    sort               INTEGER DEFAULT (0),
    createTime         INTEGER,
    updateTime         INTEGER,
    isDelete           INTEGER DEFAULT (0),
    plugin_instance_id TEXT,
    plugin_generation  INTEGER,
    plugin_xid         TEXT
);

INSERT INTO memberAuthGroup (
                                id,
                                name,
                                desc,
                                sort,
                                createTime,
                                updateTime,
                                isDelete,
                                plugin_instance_id,
                                plugin_generation,
                                plugin_xid
                            )
                            VALUES (
                                1,
                                '未分类',
                                '未分类的权限',
                                0,
                                63935000000,
                                63936000000,
                                0,
                                NULL,
                                NULL,
                                NULL
                            );

INSERT INTO memberAuthGroup (
                                id,
                                name,
                                desc,
                                sort,
                                createTime,
                                updateTime,
                                isDelete,
                                plugin_instance_id,
                                plugin_generation,
                                plugin_xid
                            )
                            VALUES (
                                2,
                                '基础功能',
                                '基础访问权限',
                                100,
                                63935000000,
                                63936000000,
                                0,
                                NULL,
                                NULL,
                                NULL
                            );

INSERT INTO memberAuthGroup (
                                id,
                                name,
                                desc,
                                sort,
                                createTime,
                                updateTime,
                                isDelete,
                                plugin_instance_id,
                                plugin_generation,
                                plugin_xid
                            )
                            VALUES (
                                3,
                                'VIP功能',
                                'VIP会员功能访问权限',
                                200,
                                63942459557,
                                63942459557,
                                0,
                                NULL,
                                NULL,
                                NULL
                            );


-- 表：memberBalanceLog
CREATE TABLE memberBalanceLog (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    memberId   INTEGER,
    type       INTEGER DEFAULT (0),
    amount     INTEGER DEFAULT (0),
    balance    INTEGER DEFAULT (0),
    remark     TEXT,
    operator   TEXT,
    createTime INTEGER
);


-- 表：memberGroup
CREATE TABLE memberGroup (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT,
    desc       TEXT,
    authList   TEXT    DEFAULT '[]',
    authLevel  INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO memberGroup (
                            id,
                            name,
                            desc,
                            authList,
                            authLevel,
                            createTime,
                            updateTime,
                            isDelete
                        )
                        VALUES (
                            1,
                            '普通用户',
                            '默认用户组，拥有基础访问权限',
                            '[1,2]',
                            0,
                            63935000000,
                            63936000000,
                            0
                        );

INSERT INTO memberGroup (
                            id,
                            name,
                            desc,
                            authList,
                            authLevel,
                            createTime,
                            updateTime,
                            isDelete
                        )
                        VALUES (
                            2,
                            'VIP用户',
                            'VIP用户组，拥有更多访问权限',
                            '[1,2,3]',
                            200,
                            63935000000,
                            63936000000,
                            0
                        );


-- 表：menu
CREATE TABLE menu (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    parent             INTEGER DEFAULT (0),
    title              TEXT,
    icon               TEXT,
    type               INTEGER DEFAULT (1),
    openType           TEXT    DEFAULT '_component',
    href               TEXT,
    sort               INTEGER DEFAULT (0),
    visible            INTEGER DEFAULT (1),
    remark             TEXT,
    createTime         INTEGER,
    updateTime         INTEGER,
    isDelete           INTEGER DEFAULT (0),
    plugin_id          TEXT,
    plugin_instance_id TEXT,
    plugin_generation  INTEGER,
    plugin_xid         TEXT
);

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     1,
                     0,
                     '附件管理',
                     'layui-icon layui-icon-upload-drag',
                     0,
                     '',
                     '',
                     200000,
                     1,
                     '附件管理目录',
                     63935100000,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     2,
                     1,
                     '附件列表',
                     'layui-icon layui-icon-file',
                     1,
                     '_component',
                     '/admin/view/attachment',
                     200100,
                     1,
                     '管理所有附件',
                     63935100000,
                     63935293314,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     3,
                     1,
                     '存储统计',
                     'layui-icon layui-icon-chart',
                     1,
                     '_iframe',
                     '/admin/view/attachment/stats',
                     200200,
                     1,
                     '查看存储统计信息',
                     63935100000,
                     63935287201,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     4,
                     1,
                     '附件设置',
                     'layui-icon layui-icon-upload',
                     1,
                     '_iframe',
                     '/admin/view/option?file=attachment.json',
                     600400,
                     1,
                     '附件上传和存储配置',
                     63935205971,
                     63942460575,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     5,
                     0,
                     '前台用户管理',
                     'layui-icon layui-icon-friends',
                     0,
                     '',
                     '',
                     300000,
                     1,
                     '前台用户管理目录',
                     63934949844,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     6,
                     5,
                     '用户管理',
                     'layui-icon layui-icon-username',
                     1,
                     '_component',
                     '/admin/view/member/user',
                     300100,
                     1,
                     '管理前台用户',
                     63934949844,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     7,
                     5,
                     '用户组管理',
                     'layui-icon layui-icon-group',
                     1,
                     '_component',
                     '/admin/view/member/group',
                     300200,
                     1,
                     '管理前台用户组',
                     63934949844,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     8,
                     5,
                     '权限分类',
                     'layui-icon layui-icon-tabs',
                     1,
                     '_component',
                     '/admin/view/member/authgroup',
                     300300,
                     1,
                     '管理前台权限分类',
                     63934949844,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     9,
                     5,
                     '权限分组',
                     'layui-icon layui-icon-vercode',
                     1,
                     '_component',
                     '/admin/view/member/auth',
                     300400,
                     1,
                     '管理前台权限分组',
                     63934949844,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     10,
                     0,
                     '后台权限管理',
                     'layui-icon layui-icon-auz',
                     0,
                     '',
                     '',
                     400000,
                     1,
                     '后台权限管理目录',
                     63933640350,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     11,
                     10,
                     '用户管理',
                     'layui-icon layui-icon-username',
                     1,
                     '_component',
                     '/admin/view/auth/user',
                     400100,
                     1,
                     '管理后台用户',
                     63933640350,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     12,
                     10,
                     '角色管理',
                     'layui-icon layui-icon-user',
                     1,
                     '_component',
                     '/admin/view/auth/role',
                     400200,
                     1,
                     '管理用户角色',
                     63933640350,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     13,
                     10,
                     '权限分类',
                     'layui-icon layui-icon-tabs',
                     1,
                     '_component',
                     '/admin/view/auth/group',
                     400300,
                     1,
                     '管理权限分类',
                     63933640350,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     14,
                     10,
                     '权限分组',
                     'layui-icon layui-icon-template',
                     1,
                     '_component',
                     '/admin/view/auth/auth',
                     400400,
                     1,
                     '管理权限分组',
                     63933640350,
                     63936000000,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     15,
                     0,
                     '插件管理',
                     'layui-icon layui-icon-app',
                     0,
                     '',
                     '',
                     550000,
                     1,
                     '插件管理目录',
                     63942444828,
                     63942461428,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     16,
                     15,
                     '插件商店',
                     'layui-icon layui-icon-cart-simple',
                     1,
                     '_component',
                     '/admin/view/plugin/store',
                     550100,
                     1,
                     '浏览远程插件商店',
                     63942444828,
                     63942461428,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     17,
                     15,
                     '已安装插件',
                     'layui-icon layui-icon-component',
                     1,
                     '_component',
                     '/admin/view/plugin/installed',
                     550200,
                     1,
                     '查看和管理已安装插件',
                     63936742499,
                     63942461428,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     18,
                     0,
                     '设置',
                     'layui-icon layui-icon-set',
                     0,
                     '',
                     '',
                     600000,
                     1,
                     '设置目录',
                     63934847322,
                     63942460483,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     19,
                     18,
                     '菜单管理',
                     'layui-icon layui-icon-spread-left',
                     1,
                     '_component',
                     '/admin/view/option/menu',
                     600100,
                     1,
                     '管理系统菜单',
                     63933640350,
                     63942460552,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     20,
                     18,
                     '接口管理',
                     'layui-icon layui-icon-website',
                     1,
                     '_component',
                     '/admin/view/auth/uris',
                     600200,
                     1,
                     '管理API接口',
                     63933640350,
                     63942460558,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     21,
                     18,
                     '全局配置',
                     'layui-icon layui-icon-set',
                     1,
                     '_iframe',
                     '/admin/view/option?file=global.json',
                     600300,
                     1,
                     '系统全局配置管理',
                     63933640350,
                     63942460564,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     22,
                     18,
                     '设置管理',
                     'layui-icon layui-icon-set-fill',
                     1,
                     '_component',
                     '/admin/view/option/files',
                     600900,
                     1,
                     '管理设置文件和配置结构',
                     63942460654,
                     63942460654,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     23,
                     0,
                     '系统日志',
                     'layui-icon layui-icon-log',
                     1,
                     '_component',
                     '/admin/view/logs',
                     700000,
                     1,
                     '查看系统日志',
                     63933640350,
                     63942460490,
                     0,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );


-- 表：plugin_dependency
CREATE TABLE plugin_dependency (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_name     TEXT    NOT NULL,
    dependency_name TEXT    NOT NULL,
    min_version     TEXT,
    max_version     TEXT,
    FOREIGN KEY (
        plugin_name
    )
    REFERENCES plugin (name) ON DELETE CASCADE
);


-- 表：plugin_generation
CREATE TABLE plugin_generation (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    instance_id     TEXT,
    generation      INTEGER,
    package_version TEXT,
    state           TEXT,
    compile_hash    TEXT,
    load_time       INTEGER,
    start_time      INTEGER,
    stop_time       INTEGER,
    health_status   TEXT,
    error_message   TEXT,
    xid             TEXT
);


-- 表：plugin_instance
CREATE TABLE plugin_instance (
    instance_id       TEXT    PRIMARY KEY,
    package_id        TEXT,
    xid               TEXT,
    instance_name     TEXT,
    mount_path        TEXT,
    data_path         TEXT,
    private_db_path   TEXT,
    enabled           INTEGER,
    installed         INTEGER,
    config_json       TEXT,
    status            TEXT,
    active_generation INTEGER,
    create_time       INTEGER,
    update_time       INTEGER
);


-- 表：plugin_migration_log
CREATE TABLE plugin_migration_log (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    instance_id    TEXT,
    migration_name TEXT,
    direction      TEXT,
    status         TEXT,
    message        TEXT,
    exec_time      INTEGER,
    xid            TEXT
);


-- 表：plugin_package
CREATE TABLE plugin_package (
    package_id    TEXT    PRIMARY KEY,
    plugin_id     TEXT,
    version       TEXT,
    source_type   TEXT,
    install_path  TEXT,
    checksum      TEXT,
    signature     TEXT,
    trust_level   TEXT,
    manifest_json TEXT,
    install_time  INTEGER,
    xid           TEXT
);


-- 表：plugin_resource
CREATE TABLE plugin_resource (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    instance_id    TEXT,
    generation     INTEGER,
    owner_scope    TEXT,
    resource_type  TEXT,
    resource_key   TEXT,
    resource_ref   TEXT,
    destroy_policy TEXT,
    create_time    INTEGER,
    status         TEXT,
    xid            TEXT
);


-- 表：plugin_runtime
CREATE TABLE plugin_runtime (
    package_id        TEXT    PRIMARY KEY,
    xid               TEXT,
    mount_path        TEXT,
    data_path         TEXT,
    private_db_path   TEXT,
    enabled           INTEGER,
    installed         INTEGER,
    config_json       TEXT,
    status            TEXT,
    active_generation INTEGER,
    create_time       INTEGER,
    update_time       INTEGER
);


-- 表：plugin_service
CREATE TABLE plugin_service (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    instance_id   TEXT,
    generation    INTEGER,
    service_name  TEXT,
    major_version INTEGER,
    minor_version INTEGER,
    status        TEXT,
    xid           TEXT
);


-- 表：role
CREATE TABLE role (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT,
    desc       TEXT,
    authList   TEXT    DEFAULT "[]",
    authLevel  INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER
);

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     '超级管理员',
                     '超级管理员账户，拥有后台的全部权限',
                     '[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]',
                     999,
                     63930031353,
                     63942461462,
                     0
                 );

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     '管理员',
                     '拥有除权限管理外的大部分权限',
                     '[1,2,3,4,5,6,7,8,15,16,17,19]',
                     200,
                     63930031443,
                     63942461507,
                     0
                 );


-- 表：uris
CREATE TABLE uris (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT
                               UNIQUE,
    authID             INTEGER,
    uri                TEXT    UNIQUE,
    desc               TEXT,
    isBackend          INTEGER DEFAULT 1,
    needAuth           INTEGER DEFAULT 1,
    needLog            INTEGER DEFAULT 0,
    keepActive         INTEGER DEFAULT 0,
    sort               INTEGER,
    createTime         INTEGER,
    updateTime         INTEGER,
    plugin_id          TEXT,
    plugin_instance_id TEXT,
    plugin_generation  INTEGER,
    plugin_xid         TEXT
);

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     1,
                     1,
                     '/api/v1/register',
                     '前台 [用户注册] 接口',
                     0,
                     0,
                     0,
                     0,
                     10000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     2,
                     1,
                     '/api/v1/login',
                     '前台 [用户登录] 接口',
                     0,
                     0,
                     0,
                     0,
                     10001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     3,
                     1,
                     '/api/v1/logout',
                     '前台 [注销登录] 接口',
                     0,
                     1,
                     0,
                     0,
                     10002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     4,
                     1,
                     '/api/v1/profile',
                     '前台 [获取当前用户信息] 接口',
                     0,
                     1,
                     0,
                     1,
                     10003,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     5,
                     1,
                     '/api/v1/profile/password',
                     '前台 [修改当前用户密码] 接口',
                     0,
                     1,
                     0,
                     1,
                     10004,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     6,
                     1,
                     '/api/v1/balance',
                     '前台 [获取当前用户余额] 接口',
                     0,
                     1,
                     0,
                     1,
                     10005,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     7,
                     1,
                     '/api/v1/balance/log',
                     '前台 [获取余额变动记录] 接口',
                     0,
                     1,
                     0,
                     1,
                     10006,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     8,
                     1,
                     '/api/v1/attachment/upload',
                     '前台 [上传附件] 接口',
                     0,
                     1,
                     0,
                     0,
                     10007,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     9,
                     1,
                     '/api/v1/attachment/my',
                     '前台 [我的附件列表] 接口',
                     0,
                     1,
                     0,
                     0,
                     10008,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     10,
                     1,
                     '/api/v1/attachment/purchase',
                     '前台 [购买附件] 接口',
                     0,
                     1,
                     1,
                     0,
                     10009,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     11,
                     1,
                     '/api/v1/attachment/purchased',
                     '前台 [已购附件列表] 接口',
                     0,
                     1,
                     0,
                     0,
                     10010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     12,
                     2,
                     '/admin/login',
                     '后台 [用户登录] 主接口',
                     1,
                     0,
                     1,
                     0,
                     1000000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     13,
                     2,
                     '/admin',
                     '后台管理首页入口',
                     1,
                     1,
                     0,
                     1,
                     1000001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     14,
                     2,
                     '/admin/view/home',
                     '后台主页视图',
                     1,
                     1,
                     0,
                     1,
                     1000002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     15,
                     2,
                     '/admin/menu',
                     '后台左侧菜单数据接口',
                     1,
                     1,
                     0,
                     0,
                     1000003,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     16,
                     2,
                     '/admin/logout',
                     '后台 [注销登录] 接口',
                     1,
                     1,
                     0,
                     0,
                     1000004,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     17,
                     3,
                     '/admin/view/attachment',
                     '[附件列表] 附件管理页面',
                     1,
                     1,
                     0,
                     1,
                     1100000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     18,
                     3,
                     '/admin/view/attachment/upload',
                     '[附件列表] 上传附件页面',
                     1,
                     1,
                     0,
                     0,
                     1100001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     19,
                     3,
                     '/admin/view/attachment/edit',
                     '[附件列表] 编辑附件页面',
                     1,
                     1,
                     0,
                     0,
                     1100002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     20,
                     3,
                     '/admin/attachment/list',
                     '[附件列表] 获取附件列表接口',
                     1,
                     1,
                     0,
                     0,
                     1100010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     21,
                     3,
                     '/admin/attachment/get',
                     '[附件列表] 获取附件详情接口',
                     1,
                     1,
                     0,
                     0,
                     1100011,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     22,
                     3,
                     '/admin/attachment/upload',
                     '[附件列表] 后台上传附件接口',
                     1,
                     1,
                     0,
                     0,
                     1100012,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     23,
                     3,
                     '/admin/attachment/save',
                     '[附件列表] 保存附件信息接口',
                     1,
                     1,
                     1,
                     0,
                     1100013,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     24,
                     3,
                     '/admin/attachment/delete',
                     '[附件列表] 删除附件接口',
                     1,
                     1,
                     1,
                     0,
                     1100014,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     25,
                     3,
                     '/attachment',
                     '[附件访问] 附件下载/预览入口',
                     1,
                     0,
                     0,
                     0,
                     1100020,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     26,
                     4,
                     '/admin/view/attachment/stats',
                     '[存储统计] 存储统计页面',
                     1,
                     1,
                     0,
                     1,
                     1101000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     27,
                     4,
                     '/admin/attachment/stats',
                     '[存储统计] 获取存储统计数据接口',
                     1,
                     1,
                     0,
                     0,
                     1101010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     28,
                     5,
                     '/admin/view/member/user',
                     '[用户管理] 用户列表页面',
                     1,
                     1,
                     0,
                     1,
                     1200000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     29,
                     5,
                     '/admin/view/member/user/add',
                     '[用户管理] 添加用户页面',
                     1,
                     1,
                     0,
                     0,
                     1200001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     30,
                     5,
                     '/admin/view/member/user/edit',
                     '[用户管理] 编辑用户页面',
                     1,
                     1,
                     0,
                     0,
                     1200002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     31,
                     5,
                     '/admin/view/member/user/balance',
                     '[用户管理] 调整用户余额页面',
                     1,
                     1,
                     0,
                     0,
                     1200003,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     32,
                     5,
                     '/admin/member/user',
                     '[用户管理] 用户CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1200010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     33,
                     5,
                     '/admin/member/user/repwd',
                     '[用户管理] 重置用户密码接口（危险）',
                     1,
                     1,
                     1,
                     1,
                     1200011,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     34,
                     5,
                     '/admin/member/user/balance',
                     '[用户管理] 调整用户余额接口（危险）',
                     1,
                     1,
                     1,
                     1,
                     1200012,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     35,
                     6,
                     '/admin/view/member/group',
                     '[用户组管理] 用户组列表页面',
                     1,
                     1,
                     0,
                     1,
                     1201000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     36,
                     6,
                     '/admin/view/member/group/add',
                     '[用户组管理] 添加用户组页面',
                     1,
                     1,
                     0,
                     0,
                     1201001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     37,
                     6,
                     '/admin/view/member/group/edit',
                     '[用户组管理] 编辑用户组页面',
                     1,
                     1,
                     0,
                     0,
                     1201002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     38,
                     6,
                     '/admin/member/group',
                     '[用户组管理] 用户组CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1201010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     39,
                     7,
                     '/admin/view/member/authgroup',
                     '[权限分类] 权限分类列表页面',
                     1,
                     1,
                     0,
                     1,
                     1202000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     40,
                     7,
                     '/admin/view/member/authgroup/add',
                     '[权限分类] 添加分类页面',
                     1,
                     1,
                     0,
                     0,
                     1202001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     41,
                     7,
                     '/admin/view/member/authgroup/edit',
                     '[权限分类] 编辑分类页面',
                     1,
                     1,
                     0,
                     0,
                     1202002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     42,
                     7,
                     '/admin/member/authgroup',
                     '[权限分类] 权限分类CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1202010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     43,
                     8,
                     '/admin/view/member/auth',
                     '[权限分组] 权限分组列表页面',
                     1,
                     1,
                     0,
                     1,
                     1203000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     44,
                     8,
                     '/admin/view/member/auth/add',
                     '[权限分组] 添加分组页面',
                     1,
                     1,
                     0,
                     0,
                     1203001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     45,
                     8,
                     '/admin/view/member/auth/edit',
                     '[权限分组] 编辑分组页面',
                     1,
                     1,
                     0,
                     0,
                     1203002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     46,
                     8,
                     '/admin/member/auth',
                     '[权限分组] 权限分组CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1203010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     47,
                     9,
                     '/admin/view/auth/user',
                     '[用户管理] 后台用户列表页面',
                     1,
                     1,
                     0,
                     1,
                     1300000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     48,
                     9,
                     '/admin/view/auth/user/add',
                     '[用户管理] 添加后台用户页面',
                     1,
                     1,
                     0,
                     0,
                     1300001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     49,
                     9,
                     '/admin/view/auth/user/edit',
                     '[用户管理] 编辑后台用户页面',
                     1,
                     1,
                     0,
                     0,
                     1300002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     50,
                     9,
                     '/admin/auth/user',
                     '[用户管理] 后台用户CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1300010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     51,
                     9,
                     '/admin/auth/user/repwd',
                     '[用户管理] 重置后台用户密码接口（危险）',
                     1,
                     1,
                     1,
                     1,
                     1300011,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     52,
                     10,
                     '/admin/view/auth/role',
                     '[角色管理] 角色列表页面',
                     1,
                     1,
                     0,
                     1,
                     1301000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     53,
                     10,
                     '/admin/view/auth/role/add',
                     '[角色管理] 添加角色页面',
                     1,
                     1,
                     0,
                     0,
                     1301001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     54,
                     10,
                     '/admin/view/auth/role/edit',
                     '[角色管理] 编辑角色页面',
                     1,
                     1,
                     0,
                     0,
                     1301002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     55,
                     10,
                     '/admin/auth/role',
                     '[角色管理] 角色CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1301010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     56,
                     11,
                     '/admin/view/auth/group',
                     '[权限分类] 权限分类列表页面',
                     1,
                     1,
                     0,
                     1,
                     1302000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     57,
                     11,
                     '/admin/view/auth/group/add',
                     '[权限分类] 添加分类页面',
                     1,
                     1,
                     0,
                     0,
                     1302001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     58,
                     11,
                     '/admin/view/auth/group/edit',
                     '[权限分类] 编辑分类页面',
                     1,
                     1,
                     0,
                     0,
                     1302002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     59,
                     11,
                     '/admin/auth/group',
                     '[权限分类] 权限分类CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1302010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     60,
                     12,
                     '/admin/view/auth/auth',
                     '[权限分组] 权限分组列表页面',
                     1,
                     1,
                     0,
                     1,
                     1303000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     61,
                     12,
                     '/admin/view/auth/auth/add',
                     '[权限分组] 添加分组页面',
                     1,
                     1,
                     0,
                     0,
                     1303001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     62,
                     12,
                     '/admin/view/auth/auth/edit',
                     '[权限分组] 编辑分组页面',
                     1,
                     1,
                     0,
                     0,
                     1303002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     63,
                     12,
                     '/admin/auth/auth',
                     '[权限分组] 权限分组CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1303010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     64,
                     13,
                     '/admin/view/plugin/store',
                     '[插件商店] 插件商店页面',
                     1,
                     1,
                     0,
                     1,
                     1400000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     65,
                     14,
                     '/admin/view/plugin/installed',
                     '[已安装插件] 已安装插件列表页面',
                     1,
                     1,
                     0,
                     1,
                     1401000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     66,
                     14,
                     '/admin/view/plugin',
                     '[已安装插件] 插件详情页面',
                     1,
                     1,
                     0,
                     0,
                     1401001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     67,
                     14,
                     '/admin/plugin/list',
                     '[已安装插件] 获取已安装插件列表接口',
                     1,
                     1,
                     0,
                     0,
                     1401010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     68,
                     14,
                     '/admin/plugin/get',
                     '[已安装插件] 获取插件详情接口',
                     1,
                     1,
                     0,
                     0,
                     1401011,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     69,
                     14,
                     '/admin/plugin/settings',
                     '[已安装插件] 保存插件设置接口',
                     1,
                     1,
                     1,
                     0,
                     1401012,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     70,
                     14,
                     '/admin/plugin/enable',
                     '[已安装插件] 启用插件接口',
                     1,
                     1,
                     1,
                     0,
                     1401013,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     71,
                     14,
                     '/admin/plugin/disable',
                     '[已安装插件] 禁用插件接口',
                     1,
                     1,
                     1,
                     0,
                     1401014,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     72,
                     14,
                     '/admin/plugin/reload',
                     '[已安装插件] 重新加载插件接口',
                     1,
                     1,
                     1,
                     0,
                     1401015,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     73,
                     15,
                     '/admin/view/option',
                     '[设置管理] 配置页面视图',
                     1,
                     1,
                     0,
                     1,
                     1500000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     74,
                     15,
                     '/admin/option',
                     '[设置管理] 配置读写主接口',
                     1,
                     1,
                     1,
                     1,
                     1500010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     75,
                     16,
                     '/admin/view/option/menu',
                     '[菜单管理] 菜单列表页面',
                     1,
                     1,
                     0,
                     1,
                     1501000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     76,
                     16,
                     '/admin/view/option/menu/add',
                     '[菜单管理] 添加菜单页面',
                     1,
                     1,
                     0,
                     0,
                     1501001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     77,
                     16,
                     '/admin/view/option/menu/add/category',
                     '[菜单管理] 添加分类页面',
                     1,
                     1,
                     0,
                     0,
                     1501002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     78,
                     16,
                     '/admin/view/option/menu/edit',
                     '[菜单管理] 编辑菜单页面',
                     1,
                     1,
                     0,
                     0,
                     1501003,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     79,
                     16,
                     '/admin/option/menu',
                     '[菜单管理] 菜单CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1501010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     80,
                     17,
                     '/admin/view/auth/uris',
                     '[接口管理] 接口列表页面',
                     1,
                     1,
                     0,
                     1,
                     1502000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     81,
                     17,
                     '/admin/view/auth/uris/edit',
                     '[接口管理] 编辑接口页面',
                     1,
                     1,
                     0,
                     0,
                     1502001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     82,
                     17,
                     '/admin/auth/uris',
                     '[接口管理] 接口CRUD主接口',
                     1,
                     1,
                     1,
                     1,
                     1502010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     83,
                     18,
                     '/admin/view/logs',
                     '[系统日志] 日志列表页面',
                     1,
                     1,
                     0,
                     1,
                     1600000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     84,
                     18,
                     '/admin/logs',
                     '[系统日志] 获取日志列表接口',
                     1,
                     1,
                     0,
                     1,
                     1600010,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     85,
                     18,
                     '/admin/logs/clear',
                     '[系统日志] 清除7天前日志接口',
                     1,
                     1,
                     1,
                     1,
                     1600011,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     86,
                     20,
                     '/admin/trace',
                     '[调试] 获取全局缓存概览',
                     1,
                     1,
                     0,
                     1,
                     1800000,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     87,
                     20,
                     '/admin/trace/session',
                     '[调试] 获取完整Session缓存数据',
                     1,
                     1,
                     0,
                     1,
                     1800001,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     88,
                     20,
                     '/admin/trace/option',
                     '[调试] 获取完整Option配置缓存',
                     1,
                     1,
                     0,
                     1,
                     1800002,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     89,
                     20,
                     '/admin/trace/auth',
                     '[调试] 获取权限相关缓存数据',
                     1,
                     1,
                     0,
                     1,
                     1800003,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     90,
                     20,
                     '/admin/trace/route',
                     '[调试] 获取路由表数据',
                     1,
                     1,
                     0,
                     1,
                     1800004,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     91,
                     15,
                     '/admin/view/option/files',
                     '[设置管理] 设置文件列表页面',
                     1,
                     1,
                     0,
                     1,
                     1500020,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     92,
                     15,
                     '/admin/view/option/file',
                     '[设置管理] 设置文件编辑页面',
                     1,
                     1,
                     0,
                     1,
                     1500021,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     93,
                     15,
                     '/admin/option/files',
                     '[设置管理] 获取设置文件列表接口',
                     1,
                     1,
                     0,
                     1,
                     1500022,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     94,
                     15,
                     '/admin/option/file',
                     '[设置管理] 设置文件 CRUD 接口',
                     1,
                     1,
                     1,
                     1,
                     1500023,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime,
                     plugin_id,
                     plugin_instance_id,
                     plugin_generation,
                     plugin_xid
                 )
                 VALUES (
                     95,
                     15,
                     '/admin/option/file/menu',
                     '[设置管理] 将设置文件加入菜单接口',
                     1,
                     1,
                     1,
                     1,
                     1500024,
                     63936000000,
                     63936000000,
                     NULL,
                     NULL,
                     NULL,
                     NULL
                 );



-- 表：user
CREATE TABLE user (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user       TEXT,
    salt       TEXT,
    pwd        TEXT,
    role       INTEGER,
    authLevel  INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER
);

INSERT INTO user (
                     id,
                     user,
                     salt,
                     pwd,
                     role,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     'admin',
                     'LedPuWu00001a1EizJ6p5UzcZ9lHN0ob',
                     '4C3B33D237D5B241D331DF014CCC51862FBB5D6879E59E9B07D110A299887FF7',
                     1,
                     0,
                     63933640350,
                     63935449430,
                     0
                 );


COMMIT TRANSACTION;

-- v3 运行时自装表（模块 Init 以 IF NOT EXISTS 幂等兜底；预置保证全新安装即完整 schema）
CREATE TABLE IF NOT EXISTS notify_message (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT DEFAULT '', content TEXT DEFAULT '', type TEXT DEFAULT 'manual', sourcePlugin TEXT DEFAULT '', bizType TEXT DEFAULT '', bizId INTEGER DEFAULT 0, actionUrl TEXT DEFAULT '', payloadJson TEXT DEFAULT '', senderAdminId INTEGER DEFAULT 0, sendType TEXT DEFAULT 'users', createTime INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS notify_recipient (id INTEGER PRIMARY KEY AUTOINCREMENT, messageId INTEGER DEFAULT 0, memberId INTEGER DEFAULT 0, isRead INTEGER DEFAULT 0, readTime INTEGER DEFAULT 0, isDelete INTEGER DEFAULT 0, deleteTime INTEGER DEFAULT 0, createTime INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS member_notify_setting (memberId INTEGER PRIMARY KEY, siteInboxEnabled INTEGER DEFAULT 1, emailEnabled INTEGER DEFAULT 1, updateTime INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS mail_task (id INTEGER PRIMARY KEY AUTOINCREMENT, memberId INTEGER DEFAULT 0, toEmail TEXT DEFAULT '', templateCode TEXT DEFAULT '', subject TEXT DEFAULT '', htmlBody TEXT DEFAULT '', textBody TEXT DEFAULT '', payloadJson TEXT DEFAULT '', sourcePlugin TEXT DEFAULT '', bizType TEXT DEFAULT '', bizId INTEGER DEFAULT 0, status TEXT DEFAULT 'pending', retryCount INTEGER DEFAULT 0, maxRetryCount INTEGER DEFAULT 0, nextRetryAt INTEGER DEFAULT 0, errorMessage TEXT DEFAULT '', createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0, sendTime INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS mail_log (id INTEGER PRIMARY KEY AUTOINCREMENT, taskId INTEGER DEFAULT 0, memberId INTEGER DEFAULT 0, toEmail TEXT DEFAULT '', status TEXT DEFAULT '', responseText TEXT DEFAULT '', errorMessage TEXT DEFAULT '', createTime INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS sched_task (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, enabled INTEGER DEFAULT 1, scheduleType TEXT NOT NULL, execType TEXT NOT NULL, shellType TEXT DEFAULT '', codeText TEXT DEFAULT '', customText TEXT DEFAULT '', cronExpr TEXT DEFAULT '', onceAt INTEGER DEFAULT 0, intervalValue INTEGER DEFAULT 0, intervalUnit TEXT DEFAULT '', startAt INTEGER DEFAULT 0, nextRunAt INTEGER DEFAULT 0, lastRunAt INTEGER DEFAULT 0, lastFinishAt INTEGER DEFAULT 0, timeoutSec INTEGER DEFAULT 300, overlapPolicy TEXT DEFAULT 'skip', misfirePolicy TEXT DEFAULT 'skip', workDir TEXT DEFAULT '', isRunning INTEGER DEFAULT 0, runningCount INTEGER DEFAULT 0, pendingRun INTEGER DEFAULT 0, parallelLimit INTEGER DEFAULT 0, retryCount INTEGER DEFAULT 0, retryDelaySec INTEGER DEFAULT 60, retryState INTEGER DEFAULT 0, lastStatus TEXT DEFAULT '', lastMessage TEXT DEFAULT '', lastExitCode INTEGER DEFAULT 0, createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0, isDelete INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS sched_run_log (id INTEGER PRIMARY KEY AUTOINCREMENT, taskId INTEGER DEFAULT 0, taskName TEXT DEFAULT '', triggerSource TEXT DEFAULT '', startTime INTEGER DEFAULT 0, finishTime INTEGER DEFAULT 0, durationMs INTEGER DEFAULT 0, status TEXT DEFAULT '', exitCode INTEGER DEFAULT 0, stdoutText TEXT DEFAULT '', stderrText TEXT DEFAULT '', message TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS standalone_page (id INTEGER PRIMARY KEY AUTOINCREMENT,title TEXT NOT NULL DEFAULT '',status TEXT NOT NULL DEFAULT 'draft',uris TEXT NOT NULL DEFAULT '',header TEXT NOT NULL DEFAULT '',useTemplate INTEGER NOT NULL DEFAULT 0,content TEXT NOT NULL DEFAULT '',cacheSeconds INTEGER NOT NULL DEFAULT 0,createTime INTEGER NOT NULL DEFAULT 0,updateTime INTEGER NOT NULL DEFAULT 0);

PRAGMA foreign_keys = on;
